#include "SemanticAnalysis/StatementNumerizer.hpp"

#include <cassert>
#include <optional>
#include <stdexcept>
#include <variant>

#include "Utils/Overload.hpp"

namespace Parsing {

std::optional<StatementNumerizer::ScopedStmtRef> StatementNumerizer::GetRef(
    const ASTNode* node) const {
  const auto ref_it = ref_by_node_.find(node);
  if (ref_it == ref_by_node_.end()) {
    return std::nullopt;
  }

  return ref_it->second;
}

const ASTNode* StatementNumerizer::GetScopeOwnerFromRef(
    const ScopedStmtRef& ref) const {
  if (ref.parent_node == nullptr) {
    return nullptr;
  }

  return ref.parent_node;
}

std::optional<StatementNumerizer::ScopedStmtRef> StatementNumerizer::ProjectUseToScope(
    ScopedStmtRef use_ref,
    const ASTNode* target_scope_owner) const {
  ScopedStmtRef ref = use_ref;
  while (ref.parent_node != nullptr) {
    if (ref.parent_node == target_scope_owner) {
      return ref;
    }

    const std::optional<ScopedStmtRef> parent_ref = GetRef(ref.parent_node);
    if (!parent_ref.has_value()) {
      throw std::runtime_error("statement numerizer is incorrect for this program");
    }
    ref = *parent_ref;
  }

  return std::nullopt;
}

void StatementNumerizer::BindNode(const ASTNode* node, const ScopedStmtRef& ref) {
  ref_by_node_[node] = ref;
}

class StatementNumerizerBuilder {
 public:
  StatementNumerizer Build(const Program& program) {
    EnterRootScope(&program);
    VisitStatements(program.top_statements);
    LeaveScope();
    return std::move(numerizer_);
  }

 private:
  using ScopedStmtRef = StatementNumerizer::ScopedStmtRef;

  const ASTNode* CurrentScopeAnchorNode() const {
    assert(!scope_anchor_stack_.empty());
    return scope_anchor_stack_.back();
  }

  void EnterRootScope(const ASTNode* scope_owner) {
    const ScopedStmtRef root_anchor{
        scope_owner,
        0,
        nullptr};
    numerizer_.BindNode(scope_owner, root_anchor);
    scope_anchor_stack_.push_back(scope_owner);
    next_stmt_id_stack_.push_back(1);
  }

  void EnterScope(const ASTNode* scope_owner) {
    assert(current_stmt_ref_.has_value());
    const ScopedStmtRef scope_anchor{
        scope_owner,
        current_stmt_ref_->stmt_id_in_scope,
        CurrentScopeAnchorNode()};
    numerizer_.BindNode(scope_owner, scope_anchor);
    scope_anchor_stack_.push_back(scope_owner);
    next_stmt_id_stack_.push_back(1);
  }

  void LeaveScope() {
    assert(!scope_anchor_stack_.empty());
    assert(!next_stmt_id_stack_.empty());
    scope_anchor_stack_.pop_back();
    next_stmt_id_stack_.pop_back();
  }

  ScopedStmtRef AcquireNextStmtRef(const ASTNode* node) {
    assert(!next_stmt_id_stack_.empty());
    const size_t statement_id = next_stmt_id_stack_.back();
    ++next_stmt_id_stack_.back();

    const ScopedStmtRef ref{
        node,
        statement_id,
        CurrentScopeAnchorNode()};
    numerizer_.BindNode(node, ref);
    return ref;
  }

  ScopedStmtRef BindNodeToCurrentStatement(const ASTNode* node) {
    assert(current_stmt_ref_.has_value());
    numerizer_.BindNode(node, *current_stmt_ref_);
    return *current_stmt_ref_;
  }

  ScopedStmtRef BindNodeWithStatementId(
      const ASTNode* node,
      size_t statement_id) {
    const ScopedStmtRef ref{
        node,
        statement_id,
        CurrentScopeAnchorNode()};
    numerizer_.BindNode(node, ref);
    return ref;
  }

  void VisitStatements(const List<Statement>& statements) {
    const std::optional<ScopedStmtRef> previous_stmt_ref = current_stmt_ref_;
    for (size_t i = 0; i < statements.size(); ++i) {
      assert(statements[i] != nullptr);
      current_stmt_ref_ = AcquireNextStmtRef(statements[i].get());
      VisitStatement(*statements[i]);
    }
    current_stmt_ref_ = previous_stmt_ref;
  }

  void VisitStatement(const Statement& statement) {
    std::visit(
        Utils::Overload{
            [this](const DeclarationStatement& declaration) {
              VisitDeclarationStatement(declaration);
            },
            [this](const FunctionDeclarationStatement& function_declaration) {
              VisitFunctionDeclaration(function_declaration);
            },
            [this](const ClassDeclarationStatement& class_declaration) {
              VisitClassDeclaration(class_declaration);
            },
            [this](const AssignmentStatement& assignment) {
              VisitAssignmentStatement(assignment);
            },
            [this](const PrintStatement& print_statement) {
              VisitPrintStatement(print_statement);
            },
            [this](const IfStatement& if_statement) {
              VisitIfStatement(if_statement);
            },
            [this](const ReturnStatement& return_statement) {
              VisitReturnStatement(return_statement);
            },
            [this](const Expression& expression) {
              VisitExpression(expression);
            },
            [this](const Block& block) {
              VisitBlock(block);
            }},
        statement.value);
  }

  void VisitDeclarationStatement(const DeclarationStatement& declaration) {
    BindNodeToCurrentStatement(&declaration);
    if (declaration.initializer != nullptr) {
      VisitExpression(*declaration.initializer);
    }
  }

  void VisitFunctionDeclaration(const FunctionDeclarationStatement& function_declaration) {
    BindNodeToCurrentStatement(&function_declaration);
    assert(function_declaration.body != nullptr);

    const std::optional<ScopedStmtRef> previous_stmt_ref = current_stmt_ref_;
    EnterScope(function_declaration.body.get());

    for (size_t i = 0; i < function_declaration.parameters.size(); ++i) {
      BindNodeWithStatementId(&function_declaration.parameters[i], 0);
    }

    VisitStatements(function_declaration.body->statements);
    LeaveScope();
    current_stmt_ref_ = previous_stmt_ref;
  }

  void VisitClassDeclaration(const ClassDeclarationStatement& class_declaration) {
    BindNodeToCurrentStatement(&class_declaration);
    const std::optional<ScopedStmtRef> previous_stmt_ref = current_stmt_ref_;

    EnterScope(&class_declaration);

    for (size_t i = 0; i < class_declaration.fields.size(); ++i) {
      current_stmt_ref_ = AcquireNextStmtRef(&class_declaration.fields[i]);
      VisitDeclarationStatement(class_declaration.fields[i]);
    }

    for (size_t i = 0; i < class_declaration.methods.size(); ++i) {
      current_stmt_ref_ = AcquireNextStmtRef(&class_declaration.methods[i]);
      VisitFunctionDeclaration(class_declaration.methods[i]);
    }

    LeaveScope();
    current_stmt_ref_ = previous_stmt_ref;
  }

  void VisitAssignmentStatement(const AssignmentStatement& assignment) {
    BindNodeToCurrentStatement(&assignment);
    assert(assignment.expr != nullptr);
    VisitExpression(*assignment.expr);
  }

  void VisitPrintStatement(const PrintStatement& print_statement) {
    BindNodeToCurrentStatement(&print_statement);
    assert(print_statement.expr != nullptr);
    VisitExpression(*print_statement.expr);
  }

  void VisitIfStatement(const IfStatement& if_statement) {
    BindNodeToCurrentStatement(&if_statement);
    assert(if_statement.condition != nullptr);
    assert(if_statement.true_block != nullptr);
    assert(if_statement.else_tail != nullptr);

    VisitExpression(*if_statement.condition);
    VisitBlock(*if_statement.true_block);
    VisitElseTail(*if_statement.else_tail);
  }

  void VisitElseTail(const ElseTail& else_tail) {
    BindNodeToCurrentStatement(&else_tail);
    if (else_tail.else_if != nullptr) {
      VisitIfStatement(*else_tail.else_if);
      return;
    }

    if (else_tail.else_block != nullptr) {
      VisitBlock(*else_tail.else_block);
    }
  }

  void VisitReturnStatement(const ReturnStatement& return_statement) {
    BindNodeToCurrentStatement(&return_statement);
    if (return_statement.expr != nullptr) {
      VisitExpression(*return_statement.expr);
    }
  }

  void VisitBlock(const Block& block) {
    BindNodeToCurrentStatement(&block);
    const std::optional<ScopedStmtRef> previous_stmt_ref = current_stmt_ref_;
    EnterScope(&block);
    VisitStatements(block.statements);
    LeaveScope();
    current_stmt_ref_ = previous_stmt_ref;
  }

  void VisitCallArgument(const CallArgument& argument) {
    std::visit(
        Utils::Overload{
            [this](const auto& argument) {
              BindNodeToCurrentStatement(&argument);
              assert(argument.value != nullptr);
              VisitExpression(*argument.value);
            }},
        argument);
  }

  void VisitExpression(const Expression& expression) {
    BindNodeToCurrentStatement(&expression);
    std::visit(
        Utils::Overload{
            [this](const IdentifierExpression& identifier_expression) {
              BindNodeToCurrentStatement(&identifier_expression);
            },
            [this](const LiteralExpression& literal_expression) {
              BindNodeToCurrentStatement(&literal_expression);
            },
            [this](const FunctionCall& function_call) {
              BindNodeToCurrentStatement(&function_call);
              for (size_t i = 0; i < function_call.arguments.size(); ++i) {
                assert(function_call.arguments[i] != nullptr);
                VisitCallArgument(*function_call.arguments[i]);
              }
            },
            [this](const FieldAccess& field_access) {
              BindNodeToCurrentStatement(&field_access);
              BindNodeToCurrentStatement(&field_access.object_name);
              BindNodeToCurrentStatement(&field_access.field_name);
            },
            [this](const MethodCall& method_call) {
              BindNodeToCurrentStatement(&method_call);
              BindNodeToCurrentStatement(&method_call.object_name);
              BindNodeToCurrentStatement(&method_call.function_call);
              for (size_t i = 0; i < method_call.function_call.arguments.size(); ++i) {
                assert(method_call.function_call.arguments[i] != nullptr);
                VisitCallArgument(*method_call.function_call.arguments[i]);
              }
            },
            [this]<UnaryExpressionNode Node>(const Node& unary_expression) {
              BindNodeToCurrentStatement(&unary_expression);
              assert(unary_expression.operand != nullptr);
              VisitExpression(*unary_expression.operand);
            },
            [this]<BinaryExpressionNode Node>(const Node& binary_expression) {
              BindNodeToCurrentStatement(&binary_expression);
              assert(binary_expression.left != nullptr);
              assert(binary_expression.right != nullptr);
              VisitExpression(*binary_expression.left);
              VisitExpression(*binary_expression.right);
            }},
        expression.value);
  }

  StatementNumerizer numerizer_;
  std::vector<const ASTNode*> scope_anchor_stack_;
  std::vector<size_t> next_stmt_id_stack_;
  std::optional<ScopedStmtRef> current_stmt_ref_;
};

StatementNumerizer BuildStatementNumerizer(const Program& program) {
  StatementNumerizerBuilder builder;
  return builder.Build(program);
}

}  // namespace Parsing
