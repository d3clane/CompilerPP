#include "SemanticAnalysis/SymbolTable.hpp"

#include <cassert>
#include <stdexcept>
#include <utility>
#include <variant>

#include "Utils/Overload.hpp"

namespace Parsing {

LocalSymbolTable::LocalSymbolTable(LocalSymbolTable* parent)
    : parent_(parent) {}

const SymbolData* LocalSymbolTable::GetSymbolInfoInLocalScope(
    const std::string& name) const {
  const auto symbol_it = symbols_.find(name);
  if (symbol_it == symbols_.end()) {
    return nullptr;
  }

  return &symbol_it->second;
}

const SymbolData* LocalSymbolTable::GetVisibleSymbolInParents(
    const std::string& name) const {
  for (const LocalSymbolTable* table = parent_; table != nullptr; table = table->parent_) {
    const SymbolData* symbol_data = table->GetSymbolInfoInLocalScope(name);
    if (symbol_data != nullptr) {
      return symbol_data;
    }
  }

  return nullptr;
}

const SymbolData* LocalSymbolTable::GetSymbolInfo(const std::string& name) const {
  const SymbolData* current_scope_symbol = GetSymbolInfoInLocalScope(name);
  if (current_scope_symbol != nullptr) {
    return current_scope_symbol;
  }

  if (parent_ == nullptr) {
    return nullptr;
  }

  return parent_->GetSymbolInfo(name);
}

void LocalSymbolTable::AddSymbolInfo(SymbolData symbol_data) {
  if (GetSymbolInfoInLocalScope(symbol_data.name) != nullptr) {
    throw std::runtime_error("Duplicate declaration in the same scope: " + symbol_data.name);
  }

  const SymbolData* visible_parent_symbol = GetVisibleSymbolInParents(symbol_data.name);
  if (visible_parent_symbol != nullptr &&
      !SymbolData::CanBeShadowed(*visible_parent_symbol)) {
    throw std::runtime_error("Symbol cannot be shadowed: " + symbol_data.name);
  }

  symbols_.emplace(symbol_data.name, std::move(symbol_data));
}

LocalSymbolTable* LocalSymbolTable::GetParent() const {
  return parent_;
}

LocalSymbolTable& SymbolTable::CreateLocalTable(LocalSymbolTable* parent) {
  owned_tables_.push_back(std::make_unique<LocalSymbolTable>(parent));
  return *owned_tables_.back();
}

void SymbolTable::AddTable(const ASTNode* node, LocalSymbolTable& table) {
  table_by_node_[node] = &table;
}

const LocalSymbolTable* SymbolTable::GetTable(const ASTNode* node) const {
  const auto table_it = table_by_node_.find(node);
  if (table_it == table_by_node_.end()) {
    return nullptr;
  }

  return table_it->second;
}

const SymbolData* SymbolTable::GetSymbolInfoInLocalScope(
    const std::string& name,
    const ASTNode* node) const {
  const LocalSymbolTable* scope = GetTable(node);
  if (scope == nullptr) {
    return nullptr;
  }

  return scope->GetSymbolInfoInLocalScope(name);
}

const SymbolData* SymbolTable::GetSymbolInfo(
    const std::string& name,
    const ASTNode* node) const {
  const LocalSymbolTable* scope = GetTable(node);
  if (scope == nullptr) {
    return nullptr;
  }

  return scope->GetSymbolInfo(name);
}

namespace {

class SymbolTableBuilder {
 public:
  SymbolTable Build(const Program& program) {
    EnterScope(&program);

    for (size_t i = 0; i < program.top_statements.size(); ++i) {
      assert(program.top_statements[i] != nullptr);
      VisitStatement(*program.top_statements[i]);
    }

    LeaveScope();
    return std::move(symbol_table_);
  }

 private:
  size_t AcquireNextInScopeStatementId() {
    assert(!next_statement_id_stack_.empty());
    size_t& next_id = next_statement_id_stack_.back();
    const size_t assigned_id = next_id;
    ++next_id;
    return assigned_id;
  }

  LocalSymbolTable& CurrentScope() {
    assert(!scope_stack_.empty());
    assert(scope_stack_.back() != nullptr);
    return *scope_stack_.back();
  }

  LocalSymbolTable& EnterScope(const ASTNode* owner_node) {
    LocalSymbolTable* parent = nullptr;
    if (!scope_stack_.empty()) {
      parent = scope_stack_.back();
    }

    const size_t first_statement_id = 1;
    LocalSymbolTable& table = symbol_table_.CreateLocalTable(parent);
    scope_stack_.push_back(&table);
    next_statement_id_stack_.push_back(first_statement_id);
    symbol_table_.AddTable(owner_node, table);
    return table;
  }

  void LeaveScope() {
    assert(!scope_stack_.empty());
    assert(!next_statement_id_stack_.empty());
    next_statement_id_stack_.pop_back();
    scope_stack_.pop_back();
  }

  void RegisterNode(const ASTNode* node) {
    symbol_table_.AddTable(node, CurrentScope());
  }

  void VisitStatement(const Statement& statement) {
    current_statement_id_ = AcquireNextInScopeStatementId();
    RegisterNode(&statement);
    std::visit(
        Utils::Overload{
            [this](const DeclarationStatement& declaration) {
              VisitDeclarationStatement(declaration);
            },
            [this](const FunctionDeclarationStatement& function_declaration) {
              VisitFunctionDeclarationStatement(function_declaration);
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
    RegisterNode(&declaration);
    if (declaration.initializer != nullptr) {
      VisitExpression(*declaration.initializer);
    }

    CurrentScope().AddSymbolInfo(SymbolData{
        declaration.type,
        declaration.variable_name,
        declaration.is_mutable,
        SymbolDebugInfo{"variable declaration"},
        &declaration,
        current_statement_id_,
        SymbolKind::Variable});
  }

  void VisitFunctionDeclarationStatement(
      const FunctionDeclarationStatement& function_declaration) {
    RegisterNode(&function_declaration);
    assert(function_declaration.body != nullptr);

    FuncType function_type;
    for (size_t i = 0; i < function_declaration.parameters.size(); ++i) {
      function_type.parameter_types.push_back(function_declaration.parameters[i].type);
    }

    if (function_declaration.return_type.has_value()) {
      function_type.return_type = *function_declaration.return_type;
    }

    CurrentScope().AddSymbolInfo(SymbolData{
        Type{std::make_shared<FuncType>(std::move(function_type))},
        function_declaration.function_name,
        false,
        SymbolDebugInfo{"function declaration"},
        &function_declaration,
        current_statement_id_,
        SymbolKind::Function});

    EnterScope(function_declaration.body.get());

    for (size_t i = 0; i < function_declaration.parameters.size(); ++i) {
      RegisterNode(&function_declaration.parameters[i]);
      CurrentScope().AddSymbolInfo(SymbolData{
          function_declaration.parameters[i].type,
          function_declaration.parameters[i].name,
          false,
          SymbolDebugInfo{"function parameter"},
          &function_declaration.parameters[i],
          0,
          SymbolKind::Parameter});
    }

    for (size_t i = 0; i < function_declaration.body->statements.size(); ++i) {
      assert(function_declaration.body->statements[i] != nullptr);
      VisitStatement(*function_declaration.body->statements[i]);
    }

    LeaveScope();
  }

  void VisitAssignmentStatement(const AssignmentStatement& assignment) {
    RegisterNode(&assignment);
    assert(assignment.expr != nullptr);
    VisitExpression(*assignment.expr);
  }

  void VisitPrintStatement(const PrintStatement& print_statement) {
    RegisterNode(&print_statement);
    assert(print_statement.expr != nullptr);
    VisitExpression(*print_statement.expr);
  }

  void VisitIfStatement(const IfStatement& if_statement) {
    RegisterNode(&if_statement);
    assert(if_statement.condition != nullptr);
    assert(if_statement.true_block != nullptr);
    assert(if_statement.else_tail != nullptr);

    VisitExpression(*if_statement.condition);
    VisitBlock(*if_statement.true_block);
    VisitElseTail(*if_statement.else_tail);
  }

  void VisitElseTail(const ElseTail& else_tail) {
    RegisterNode(&else_tail);
    assert(!(else_tail.else_if != nullptr && else_tail.else_block != nullptr));

    if (else_tail.else_if != nullptr) {
      VisitIfStatement(*else_tail.else_if);
      return;
    }

    if (else_tail.else_block != nullptr) {
      VisitBlock(*else_tail.else_block);
    }
  }

  void VisitReturnStatement(const ReturnStatement& return_statement) {
    RegisterNode(&return_statement);
    if (return_statement.expr != nullptr) {
      VisitExpression(*return_statement.expr);
    }
  }

  void VisitBlock(const Block& block) {
    EnterScope(&block);
    for (size_t i = 0; i < block.statements.size(); ++i) {
      assert(block.statements[i] != nullptr);
      VisitStatement(*block.statements[i]);
    }
    LeaveScope();
  }

  void VisitExpression(const Expression& expression) {
    RegisterNode(&expression);
    std::visit(
        Utils::Overload{
            [this](const IdentifierExpression& identifier_expression) {
              RegisterNode(&identifier_expression);
            },
            [this](const LiteralExpression& literal_expression) {
              RegisterNode(&literal_expression);
            },
            [this](const FunctionCall& function_call) {
              RegisterNode(&function_call);

              for (size_t i = 0; i < function_call.arguments.size(); ++i) {
                assert(function_call.arguments[i] != nullptr);
                std::visit(
                    Utils::Overload{
                        [this](const auto& argument) {
                          RegisterNode(&argument);
                          assert(argument.value != nullptr);
                          VisitExpression(*argument.value);
                        }},
                    *function_call.arguments[i]);
              }
            },
            [this]<UnaryExpressionNode Node>(const Node& expression_node) {
              VisitUnaryExpressionNode(expression_node);
            },
            [this]<BinaryExpressionNode Node>(const Node& expression_node) {
              VisitBinaryExpressionNode(expression_node);
            }},
        expression.value);
  }

  void VisitUnaryExpressionNode(const UnaryOperationBase& unary_expression) {
    RegisterNode(&unary_expression);
    assert(unary_expression.operand != nullptr);
    VisitExpression(*unary_expression.operand);
  }

  void VisitBinaryExpressionNode(const BinaryOperationBase& binary_expression) {
    RegisterNode(&binary_expression);
    assert(binary_expression.left != nullptr);
    assert(binary_expression.right != nullptr);
    VisitExpression(*binary_expression.left);
    VisitExpression(*binary_expression.right);
  }

  SymbolTable symbol_table_;
  std::vector<LocalSymbolTable*> scope_stack_;
  std::vector<size_t> next_statement_id_stack_;
  size_t current_statement_id_ = 0;
};

}  // namespace

SymbolTable BuildSymbolTable(const Program& program) {
  SymbolTableBuilder builder;
  return builder.Build(program);
}

}  // namespace Parsing
