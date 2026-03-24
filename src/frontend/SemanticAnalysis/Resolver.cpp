#include "SemanticAnalysis/Resolver.hpp"

#include <cassert>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

#include "Utils/Overload.hpp"

namespace Parsing {

namespace {

constexpr AstNodeID kInvalidAstNodeID = std::numeric_limits<AstNodeID>::max();

struct LocalUse {
  AstNodeID node_id;
  std::string used_ident_name;

  friend bool operator<(const LocalUse& left, const LocalUse& right) {
    if (left.node_id != right.node_id) {
      return left.node_id < right.node_id;
    }

    return left.used_ident_name < right.used_ident_name;
  }
};

class UseResolverBuilder {
 public:
  UseResolverBuilder(const Program& program, const SymbolTable& symbol_table)
      : program_(program),
        symbol_table_(symbol_table) {}

  std::map<LocalUse, AstNodeID> Build() {
    VisitStatements(program_.top_statements);
    return std::move(use_to_definition_);
  }

 private:
  AstNodeID ResolveUse(
      const std::string& name,
      const ASTNode& use_node,
      size_t in_scope_stmt_id) const {
    const LocalSymbolTable* scope = symbol_table_.GetTable(use_node);
    if (scope == nullptr) {
      throw std::runtime_error("symbol table is incorrect for this program");
    }

    const SymbolData* local_symbol =
        symbol_table_.GetSymbolInfoInLocalScope(name, use_node);
    if (local_symbol != nullptr) {
      if (local_symbol->kind == SymbolKind::Function ||
          local_symbol->in_scope_stmt_id < in_scope_stmt_id) {
        return local_symbol->declaration_node_id;
      }

      const LocalSymbolTable* parent_scope = scope->GetParent();
      if (parent_scope == nullptr) {
        throw std::runtime_error("use before def");
      }

      const SymbolData* parent_symbol = parent_scope->GetSymbolInfo(name);
      if (parent_symbol == nullptr) {
        throw std::runtime_error("use before def");
      }

      return parent_symbol->declaration_node_id;
    }

    const SymbolData* visible_symbol = scope->GetSymbolInfo(name);
    if (visible_symbol == nullptr) {
      throw std::runtime_error("use before def");
    }

    return visible_symbol->declaration_node_id;
  }

  void RegisterUse(
      const ASTNode& use_node,
      const std::string& name,
      size_t in_scope_stmt_id) {
    const AstNodeID resolved_definition_id = ResolveUse(name, use_node, in_scope_stmt_id);
    use_to_definition_.emplace(
        LocalUse{use_node.GetId(), name},
        resolved_definition_id);
  }

  void VisitStatements(const List<Statement>& statements) {
    for (size_t i = 0; i < statements.size(); ++i) {
      assert(statements[i] != nullptr);
      // TODO: bad decision to use it separately from SymbolTable.
      // Changing numeration in SymbolTable would require changing it here as well, which is error-prone.
      const size_t in_scope_stmt_id = i + 1;
      VisitStatement(*statements[i], in_scope_stmt_id);
    }
  }

  void VisitStatement(const Statement& statement, size_t in_scope_stmt_id) {
    std::visit(
        Utils::Overload{
            [this, in_scope_stmt_id](const DeclarationStatement& declaration) {
              VisitDeclarationStatement(declaration, in_scope_stmt_id);
            },
            [this](const FunctionDeclarationStatement& function_declaration) {
              VisitFunctionDeclaration(function_declaration);
            },
            [this, in_scope_stmt_id](const AssignmentStatement& assignment) {
              VisitAssignmentStatement(assignment, in_scope_stmt_id);
            },
            [this, in_scope_stmt_id](const PrintStatement& print_statement) {
              VisitPrintStatement(print_statement, in_scope_stmt_id);
            },
            [this, in_scope_stmt_id](const IfStatement& if_statement) {
              VisitIfStatement(if_statement, in_scope_stmt_id);
            },
            [this, in_scope_stmt_id](const ReturnStatement& return_statement) {
              VisitReturnStatement(return_statement, in_scope_stmt_id);
            },
            [this, in_scope_stmt_id](const Expression& expression) {
              VisitExpression(expression, in_scope_stmt_id);
            },
            [this](const Block& block) {
              VisitBlock(block);
            }},
        statement.value);
  }

  void VisitDeclarationStatement(
      const DeclarationStatement& declaration,
      size_t in_scope_stmt_id) {
    if (declaration.initializer != nullptr) {
      VisitExpression(*declaration.initializer, in_scope_stmt_id);
    }
  }

  void VisitFunctionDeclaration(const FunctionDeclarationStatement& function_declaration) {
    assert(function_declaration.body != nullptr);
    VisitStatements(function_declaration.body->statements);
  }

  void VisitAssignmentStatement(
      const AssignmentStatement& assignment,
      size_t in_scope_stmt_id) {
    RegisterUse(assignment, assignment.variable_name, in_scope_stmt_id);

    assert(assignment.expr != nullptr);
    VisitExpression(*assignment.expr, in_scope_stmt_id);
  }

  void VisitPrintStatement(const PrintStatement& print_statement, size_t in_scope_stmt_id) {
    assert(print_statement.expr != nullptr);
    VisitExpression(*print_statement.expr, in_scope_stmt_id);
  }

  void VisitIfStatement(const IfStatement& if_statement, size_t in_scope_stmt_id) {
    assert(if_statement.condition != nullptr);
    assert(if_statement.true_block != nullptr);
    assert(if_statement.else_tail != nullptr);

    VisitExpression(*if_statement.condition, in_scope_stmt_id);
    VisitBlock(*if_statement.true_block);
    VisitElseTail(*if_statement.else_tail, in_scope_stmt_id);
  }

  void VisitElseTail(const ElseTail& else_tail, size_t in_scope_stmt_id) {
    if (else_tail.else_if != nullptr) {
      VisitIfStatement(*else_tail.else_if, in_scope_stmt_id);
      return;
    }

    if (else_tail.else_block != nullptr) {
      VisitBlock(*else_tail.else_block);
    }
  }

  void VisitReturnStatement(const ReturnStatement& return_statement, size_t in_scope_stmt_id) {
    if (return_statement.expr != nullptr) {
      VisitExpression(*return_statement.expr, in_scope_stmt_id);
    }
  }

  void VisitBlock(const Block& block) {
    VisitStatements(block.statements);
  }

  void VisitCallArgument(const CallArgument& argument, size_t in_scope_stmt_id) {
    std::visit(
        Utils::Overload{
          [this, in_scope_stmt_id](const auto& argument) {
            assert(argument.value != nullptr);
            VisitExpression(*argument.value, in_scope_stmt_id);
          }},
        argument);
  }

  void VisitExpression(const Expression& expression, size_t in_scope_stmt_id) {
    std::visit(
        Utils::Overload{
            [this, in_scope_stmt_id](const IdentifierExpression& identifier_expression) {
              RegisterUse(identifier_expression, identifier_expression.name, in_scope_stmt_id);
            },
            [](const LiteralExpression&) {
            },
            [this, in_scope_stmt_id](const FunctionCall& function_call) {
              RegisterUse(function_call, function_call.function_name, in_scope_stmt_id);
              for (size_t i = 0; i < function_call.arguments.size(); ++i) {
                assert(function_call.arguments[i] != nullptr);
                VisitCallArgument(*function_call.arguments[i], in_scope_stmt_id);
              }
            },
            [this, in_scope_stmt_id]<UnaryExpressionNode Node>(const Node& unary_expression) {
              assert(unary_expression.operand != nullptr);
              VisitExpression(*unary_expression.operand, in_scope_stmt_id);
            },
            [this, in_scope_stmt_id]<BinaryExpressionNode Node>(const Node& binary_expression) {
              assert(binary_expression.left != nullptr);
              assert(binary_expression.right != nullptr);
              VisitExpression(*binary_expression.left, in_scope_stmt_id);
              VisitExpression(*binary_expression.right, in_scope_stmt_id);
            }},
        expression.value);
  }

  const Program& program_;
  const SymbolTable& symbol_table_;
  std::map<LocalUse, AstNodeID> use_to_definition_;
};

}  // namespace

AstNodeID UseResolver::GetUsedVarDef(std::string_view name, ASTNode* curr_node) const {
  if (curr_node == nullptr) {
    return kInvalidAstNodeID;
  }

  return GetUsedVarDef(name, curr_node->GetId());
}

AstNodeID UseResolver::GetUsedVarDef(std::string_view name, AstNodeID curr_node_id) const {
  const auto definition_it =
      use_to_definition_.find(Use{curr_node_id, std::string(name)});
  if (definition_it == use_to_definition_.end()) {
    return kInvalidAstNodeID;
  }

  return definition_it->second;
}

UseResolver BuildUseResolver(const Program& program, SymbolTable& symbol_table) {
  if (symbol_table.GetTable(program) == nullptr) {
    throw std::runtime_error(
        "BuildUseResolver requires a symbol table built for the same program");
  }

  UseResolver resolver;
  UseResolverBuilder builder(program, symbol_table);
  const std::map<LocalUse, AstNodeID> use_to_definition = builder.Build();
  for (const auto& [local_use, definition_node_id] : use_to_definition) {
    resolver.use_to_definition_.emplace(
        UseResolver::Use{local_use.node_id, local_use.used_ident_name},
        definition_node_id);
  }

  return resolver;
}

}  // namespace Parsing
