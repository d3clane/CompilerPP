#include "SemanticAnalysis/AccessAllowanceChecker.hpp"

#include <cassert>
#include <map>
#include <set>
#include <stdexcept>
#include <variant>

#include "Debug/DebugCtx.hpp"
#include "Utils/Overload.hpp"

namespace Parsing {

namespace {

class AccessAllowanceCheckerVisitor {
 public:
  AccessAllowanceCheckerVisitor(
      const Program& program,
      const UseResolver& use_resolver,
      const TypeDefiner& type_definer,
      DebugCtx& debug_ctx)
      : program_(program),
        use_resolver_(use_resolver),
        type_definer_(type_definer),
        debug_ctx_(debug_ctx) {
    IndexFieldOwners();
  }

  void Check() {
    VisitStatements(program_.top_statements);
  }

 private:
  void ReportCodeError(const ASTNode* node, const std::string& message) {
    debug_ctx_.GetErrors().AddError(node, message);
  }

  void IndexFieldOwners() {
    for (const auto& [_, class_declaration] :
         type_definer_.GetClassDeclarations()) {
      if (class_declaration == nullptr) {
        continue;
      }

      for (size_t i = 0; i < class_declaration->fields.size(); ++i) {
        field_owner_by_field_node_[&class_declaration->fields[i]] =
            class_declaration;
      }
    }
  }

  const UseResolver::ResolvedSymbol& ResolveFieldSymbolOrThrow(
      const FieldAccess& field_access) const {
    const UseResolver::ResolvedSymbol* resolved_symbol =
        use_resolver_.GetResolvedSymbol(
            field_access.field_name.name,
            &field_access.field_name);
    if (resolved_symbol == nullptr) {
      throw std::runtime_error("resolver is incorrect for this program");
    }

    if (resolved_symbol->symbol_data.kind != SymbolKind::Variable) {
      throw std::runtime_error("resolver is incorrect for this program");
    }

    return *resolved_symbol;
  }

  const ClassDeclarationStatement* GetFieldOwnerClassOrThrow(
      const ASTNode* field_node) const {
    const auto field_owner_it = field_owner_by_field_node_.find(field_node);
    if (field_owner_it == field_owner_by_field_node_.end()) {
      throw std::runtime_error("resolver is incorrect for this program");
    }

    return field_owner_it->second;
  }

  bool IsSameOrDerivedClass(
      const ClassDeclarationStatement& derived_class,
      const ClassDeclarationStatement& base_class) const {
    if (derived_class.class_name == base_class.class_name) {
      return true;
    }

    std::set<std::string> visited_classes;
    const ClassDeclarationStatement* current_class = &derived_class;
    while (current_class != nullptr) {
      if (!visited_classes.insert(current_class->class_name).second) {
        return false;
      }

      if (!current_class->base_class_name.has_value()) {
        return false;
      }

      const ClassDeclarationStatement* parent_class =
          type_definer_.GetClassDeclaration(*current_class->base_class_name);
      if (parent_class == nullptr) {
        return false;
      }

      if (parent_class->class_name == base_class.class_name) {
        return true;
      }

      current_class = parent_class;
    }

    return false;
  }

  void CheckFieldAccess(const FieldAccess& field_access) {
    const UseResolver::ResolvedSymbol& resolved_field =
        ResolveFieldSymbolOrThrow(field_access);
    const ClassDeclarationStatement* field_owner_class =
        GetFieldOwnerClassOrThrow(resolved_field.definition_node);

    if (current_method_owner_class_ == nullptr) {
      ReportCodeError(
          &field_access,
          "Field access is allowed only inside class methods");
      return;
    }

    if (!IsSameOrDerivedClass(*current_method_owner_class_, *field_owner_class)) {
      ReportCodeError(
          &field_access,
          "Field access is allowed only for current class or its base classes");
    }
  }

  void VisitStatements(const List<Statement>& statements) {
    for (size_t i = 0; i < statements.size(); ++i) {
      assert(statements[i] != nullptr);
      VisitStatement(*statements[i]);
    }
  }

  void VisitStatement(const Statement& statement) {
    std::visit(
        Utils::Overload{
            [this](const DeclarationStatement& declaration) {
              if (declaration.initializer != nullptr) {
                VisitExpression(*declaration.initializer);
              }
            },
            [this](const FunctionDeclarationStatement& function_declaration) {
              VisitFunctionDeclarationStatement(function_declaration);
            },
            [this](const ClassDeclarationStatement& class_declaration) {
              VisitClassDeclarationStatement(class_declaration);
            },
            [this](const AssignmentStatement& assignment) {
              assert(assignment.expr != nullptr);
              VisitExpression(*assignment.expr);
            },
            [this](const PrintStatement& print_statement) {
              assert(print_statement.expr != nullptr);
              VisitExpression(*print_statement.expr);
            },
            [this](const IfStatement& if_statement) {
              VisitIfStatement(if_statement);
            },
            [this](const ReturnStatement& return_statement) {
              if (return_statement.expr != nullptr) {
                VisitExpression(*return_statement.expr);
              }
            },
            [this](const Expression& expression) {
              VisitExpression(expression);
            },
            [this](const Block& block) {
              VisitStatements(block.statements);
            }},
        statement.value);
  }

  void VisitFunctionDeclarationStatement(
      const FunctionDeclarationStatement& function_declaration) {
    assert(function_declaration.body != nullptr);
    const ClassDeclarationStatement* previous_method_owner_class =
        current_method_owner_class_;
    current_method_owner_class_ = nullptr;
    VisitStatements(function_declaration.body->statements);
    current_method_owner_class_ = previous_method_owner_class;
  }

  void VisitMethodDeclarationStatement(
      const FunctionDeclarationStatement& method_declaration,
      const ClassDeclarationStatement& class_declaration) {
    assert(method_declaration.body != nullptr);
    const ClassDeclarationStatement* previous_method_owner_class =
        current_method_owner_class_;
    current_method_owner_class_ = &class_declaration;
    VisitStatements(method_declaration.body->statements);
    current_method_owner_class_ = previous_method_owner_class;
  }

  void VisitClassDeclarationStatement(
      const ClassDeclarationStatement& class_declaration) {
    for (size_t i = 0; i < class_declaration.fields.size(); ++i) {
      if (class_declaration.fields[i].initializer != nullptr) {
        VisitExpression(*class_declaration.fields[i].initializer);
      }
    }

    for (size_t i = 0; i < class_declaration.methods.size(); ++i) {
      VisitMethodDeclarationStatement(class_declaration.methods[i], class_declaration);
    }
  }

  void VisitIfStatement(const IfStatement& if_statement) {
    assert(if_statement.condition != nullptr);
    assert(if_statement.true_block != nullptr);
    assert(if_statement.else_tail != nullptr);

    VisitExpression(*if_statement.condition);
    VisitStatements(if_statement.true_block->statements);
    VisitElseTail(*if_statement.else_tail);
  }

  void VisitElseTail(const ElseTail& else_tail) {
    if (else_tail.else_if != nullptr) {
      VisitIfStatement(*else_tail.else_if);
      return;
    }

    if (else_tail.else_block != nullptr) {
      VisitStatements(else_tail.else_block->statements);
    }
  }

  void VisitCallArgument(const CallArgument& argument) {
    std::visit(
        Utils::Overload{
            [this](const auto& argument) {
              assert(argument.value != nullptr);
              VisitExpression(*argument.value);
            }},
        argument);
  }

  void VisitExpression(const Expression& expression) {
    std::visit(
        Utils::Overload{
            [](const IdentifierExpression&) {
            },
            [](const LiteralExpression&) {
            },
            [this](const FunctionCall& function_call) {
              for (size_t i = 0; i < function_call.arguments.size(); ++i) {
                assert(function_call.arguments[i] != nullptr);
                VisitCallArgument(*function_call.arguments[i]);
              }
            },
            [this](const FieldAccess& field_access) {
              CheckFieldAccess(field_access);
            },
            [this](const MethodCall& method_call) {
              for (size_t i = 0; i < method_call.function_call.arguments.size(); ++i) {
                assert(method_call.function_call.arguments[i] != nullptr);
                VisitCallArgument(*method_call.function_call.arguments[i]);
              }
            },
            [this]<UnaryExpressionNode Node>(const Node& unary_expression) {
              assert(unary_expression.operand != nullptr);
              VisitExpression(*unary_expression.operand);
            },
            [this]<BinaryExpressionNode Node>(const Node& binary_expression) {
              assert(binary_expression.left != nullptr);
              assert(binary_expression.right != nullptr);
              VisitExpression(*binary_expression.left);
              VisitExpression(*binary_expression.right);
            }},
        expression.value);
  }

  const Program& program_;
  const UseResolver& use_resolver_;
  const TypeDefiner& type_definer_;
  DebugCtx& debug_ctx_;
  std::map<const ASTNode*, const ClassDeclarationStatement*> field_owner_by_field_node_;
  const ClassDeclarationStatement* current_method_owner_class_ = nullptr;
};

}  // namespace

void CheckAccessAllowance(
    const Program& program,
    const UseResolver& use_resolver,
    const TypeDefiner& type_definer,
    DebugCtx& debug_ctx) {
  AccessAllowanceCheckerVisitor visitor(
      program,
      use_resolver,
      type_definer,
      debug_ctx);
  visitor.Check();
}

void CheckAccessAllowance(
    const Program& program,
    const UseResolver& use_resolver,
    const TypeDefiner& type_definer) {
  DebugCtx debug_ctx;
  CheckAccessAllowance(
      program,
      use_resolver,
      type_definer,
      debug_ctx);
  if (debug_ctx.GetErrors().HasErrors()) {
    debug_ctx.GetErrors().ThrowErrors();
  }
}

}  // namespace Parsing
