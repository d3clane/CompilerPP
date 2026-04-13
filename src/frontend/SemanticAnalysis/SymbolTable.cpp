#include "SemanticAnalysis/SymbolTable.hpp"

#include <cassert>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

#include "Debug/DebugCtx.hpp"
#include "Utils/Overload.hpp"

namespace Front {

const ASTNode* SymbolData::GetDeclarationNode() const {
  if (declaration_ref.node == nullptr) {
    return nullptr;
  }

  if (kind == SymbolKind::Parameter) {
    return declaration_ref.node;
  }

  const auto* declaration_statement =
      dynamic_cast<const Statement*>(declaration_ref.node);
  if (declaration_statement == nullptr) {
    return declaration_ref.node;
  }

  return std::visit(
      Utils::Overload{
          [](const auto& node) -> const ASTNode* { return &node; }
      },
      declaration_statement->value);
}

SymbolData SymbolData::CreateVariableSymbolData(
    const DeclarationStatement& declaration,
    StatementNumerizer::ScopedStmtRef declaration_ref) {
  return SymbolData{
      declaration.type,
      declaration_ref,
      SymbolKind::Variable};
}

SymbolData SymbolData::CreateFunctionSymbolData(
    const FunctionDeclarationStatement& function_declaration,
    StatementNumerizer::ScopedStmtRef declaration_ref) {
  return SymbolData{
      function_declaration.function_type,
      declaration_ref,
      SymbolKind::Function};
}

SymbolData SymbolData::CreateParameterSymbolData(
    const FunctionParameter& parameter,
    StatementNumerizer::ScopedStmtRef declaration_ref) {
  return SymbolData{
      parameter.type,
      declaration_ref,
      SymbolKind::Parameter};
}

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

LocalSymbolTable::ErrorMsg LocalSymbolTable::AddSymbolInfo(
    const std::string& name,
    SymbolData symbol_data) {
  if (GetSymbolInfoInLocalScope(name) != nullptr) {
    return "Duplicate declaration in the same scope: " + name;
  }

  const SymbolData* visible_parent_symbol = GetVisibleSymbolInParents(name);
  if (visible_parent_symbol != nullptr &&
      !SymbolData::CanBeShadowed(*visible_parent_symbol)) {
    return "Symbol cannot be shadowed: " + name;
  }

  symbols_.emplace(name, std::move(symbol_data));
  return {};
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

void SymbolTable::SetScopeOwner(const ASTNode* owner, LocalSymbolTable& table) {
  scope_owner_by_table_[&table] = owner;
}

const LocalSymbolTable* SymbolTable::GetTable(const ASTNode* node) const {
  const auto table_it = table_by_node_.find(node);
  if (table_it == table_by_node_.end()) {
    return nullptr;
  }

  return table_it->second;
}

const ASTNode* SymbolTable::GetScopeOwner(const LocalSymbolTable* table) const {
  if (table == nullptr) {
    return nullptr;
  }

  const auto owner_it = scope_owner_by_table_.find(table);
  if (owner_it == scope_owner_by_table_.end()) {
    return nullptr;
  }

  return owner_it->second;
}

const ClassDeclarationStatement* SymbolTable::GetIfClassDeclarationOwner(
    const LocalSymbolTable* table) const {
  return dynamic_cast<const ClassDeclarationStatement*>(GetScopeOwner(table));
}

const FunctionDeclarationStatement* SymbolTable::GetIfFunctionDeclarationOwner(
    const LocalSymbolTable* table) const {
  return dynamic_cast<const FunctionDeclarationStatement*>(GetScopeOwner(table));
}

void SymbolTable::SetStatementNumerizer(StatementNumerizer numerizer) {
  statement_numerizer_ =
      std::make_unique<StatementNumerizer>(std::move(numerizer));
}

const StatementNumerizer* SymbolTable::GetStatementNumerizer() const {
  return statement_numerizer_.get();
}

namespace {

class SymbolTableBuilder {
 public:
  SymbolTableBuilder(
      const StatementNumerizer& numerizer,
      DebugCtx& debug_ctx)
      : numerizer_(numerizer),
        debug_ctx_(debug_ctx) {}

  SymbolTable Build(const Program& program) {
    EnterScope(&program);
    VisitStatementsInCurrentScope(program.top_statements);
    LeaveScope();
    return std::move(symbol_table_);
  }

 private:
  using ScopedStmtRef = StatementNumerizer::ScopedStmtRef;

  ScopedStmtRef GetNodeRefOrThrow(const ASTNode* node) const {
    const std::optional<ScopedStmtRef> ref = numerizer_.GetRef(node);
    if (!ref.has_value()) {
      throw std::runtime_error("statement numerizer is incorrect for this program");
    }

    return *ref;
  }

  LocalSymbolTable& CurrentScope() {
    assert(!scope_stack_.empty());
    assert(scope_stack_.back() != nullptr);
    return *scope_stack_.back();
  }

  template <typename OwnerNode>
  LocalSymbolTable& EnterScope(const OwnerNode* owner_node) {
    LocalSymbolTable* parent = nullptr;
    if (!scope_stack_.empty()) {
      parent = scope_stack_.back();
    }

    LocalSymbolTable& table = symbol_table_.CreateLocalTable(parent);
    scope_stack_.push_back(&table);
    symbol_table_.SetScopeOwner(owner_node, table);
    symbol_table_.AddTable(owner_node, table);
    return table;
  }

  void LeaveScope() {
    assert(!scope_stack_.empty());
    scope_stack_.pop_back();
  }

  template <typename OwnerNode>
  void VisitBlockWithOwner(const Block& block, const OwnerNode* owner_node) {
    EnterScope(&block);
    symbol_table_.SetScopeOwner(owner_node, CurrentScope());
    VisitStatementsInCurrentScope(block.statements);
    LeaveScope();
  }

  void RegisterNode(const ASTNode* node) {
    symbol_table_.AddTable(node, CurrentScope());
  }

  void TryAddSymbolInfo(
      const ASTNode* node,
      const std::string& symbol_name,
      SymbolData symbol_data) {
    const LocalSymbolTable::ErrorMsg error_msg =
        CurrentScope().AddSymbolInfo(symbol_name, std::move(symbol_data));
    if (!error_msg.empty()) {
      debug_ctx_.GetErrors().AddError(node, error_msg);
    }
  }

  void TryAddVariableSymbolInfo(const DeclarationStatement& declaration) {
    TryAddSymbolInfo(
        &declaration,
        declaration.variable_name.name,
        SymbolData::CreateVariableSymbolData(
            declaration,
            GetNodeRefOrThrow(&declaration)));
  }

  void TryAddFunctionSymbolInfo(
      const FunctionDeclarationStatement& function_declaration) {
    TryAddSymbolInfo(
        &function_declaration,
        function_declaration.function_name.name,
        SymbolData::CreateFunctionSymbolData(
            function_declaration,
            GetNodeRefOrThrow(&function_declaration)));
  }

  void TryAddParameterSymbolInfo(const FunctionParameter& parameter) {
    TryAddSymbolInfo(
        &parameter,
        parameter.name.name,
        SymbolData::CreateParameterSymbolData(
            parameter,
            GetNodeRefOrThrow(&parameter)));
  }

  void VisitStatementsInCurrentScope(const List<Statement>& statements) {
    for (size_t i = 0; i < statements.size(); ++i) {
      assert(statements[i] != nullptr);
      VisitStatement(*statements[i]);
    }
  }

  void VisitStatement(const Statement& statement) {
    RegisterNode(&statement);
    std::visit(
        Utils::Overload{
            [this](const DeclarationStatement& declaration) {
              VisitDeclarationStatement(declaration);
            },
            [this](const FunctionDeclarationStatement& function_declaration) {
              VisitFunctionDeclarationStatement(function_declaration);
            },
            [this](const ClassDeclarationStatement& class_declaration) {
              VisitClassDeclarationStatement(class_declaration);
            },
            [this](const AssignmentStatement& assignment) {
              VisitAssignmentStatement(assignment);
            },
            [this](const PrintStatement& print_statement) {
              VisitPrintStatement(print_statement);
            },
            [this](const DeleteStatement& delete_statement) {
              VisitDeleteStatement(delete_statement);
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

    TryAddVariableSymbolInfo(declaration);
  }

  void VisitFunctionDeclarationStatement(
      const FunctionDeclarationStatement& function_declaration) {
    RegisterNode(&function_declaration);
    TryAddFunctionSymbolInfo(function_declaration);
    VisitFunctionBody(function_declaration);
  }

  void VisitFunctionBody(const FunctionDeclarationStatement& function_declaration) {
    assert(function_declaration.body != nullptr);

    EnterScope(function_declaration.body.get());
    symbol_table_.SetScopeOwner(&function_declaration, CurrentScope());

    for (size_t i = 0; i < function_declaration.parameters.size(); ++i) {
      RegisterNode(&function_declaration.parameters[i]);
      TryAddParameterSymbolInfo(function_declaration.parameters[i]);
    }

    VisitStatementsInCurrentScope(function_declaration.body->statements);
    LeaveScope();
  }

  void VisitClassDeclarationStatement(
      const ClassDeclarationStatement& class_declaration) {
    EnterScope(&class_declaration);

    for (size_t i = 0; i < class_declaration.fields.size(); ++i) {
      VisitDeclarationStatement(class_declaration.fields[i]);
    }

    for (size_t i = 0; i < class_declaration.methods.size(); ++i) {
      VisitFunctionDeclarationStatement(class_declaration.methods[i]);
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

  void VisitDeleteStatement(const DeleteStatement& delete_statement) {
    RegisterNode(&delete_statement);
    RegisterNode(&delete_statement.variable);
  }

  void VisitIfStatement(const IfStatement& if_statement) {
    RegisterNode(&if_statement);
    assert(if_statement.condition != nullptr);
    assert(if_statement.true_block != nullptr);
    assert(if_statement.else_tail != nullptr);

    VisitExpression(*if_statement.condition);
    VisitBlockWithOwner(*if_statement.true_block, &if_statement);
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
      VisitBlockWithOwner(*else_tail.else_block, &else_tail);
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
    VisitStatementsInCurrentScope(block.statements);
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
              VisitFunctionCall(function_call);
            },
            [this](const FieldAccess& field_access) {
              RegisterNode(&field_access);
              RegisterNode(&field_access.object_name);
              RegisterNode(&field_access.field_name);
            },
            [this](const MethodCall& method_call) {
              RegisterNode(&method_call);
              RegisterNode(&method_call.object_name);
              VisitFunctionCall(method_call.function_call);
            },
            [this]<UnaryExpressionNode Node>(const Node& expression_node) {
              VisitUnaryExpressionNode(expression_node);
            },
            [this]<BinaryExpressionNode Node>(const Node& expression_node) {
              VisitBinaryExpressionNode(expression_node);
            }},
        expression.value);
  }

  void VisitFunctionCall(const FunctionCall& function_call) {
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

  const StatementNumerizer& numerizer_;
  SymbolTable symbol_table_;
  DebugCtx& debug_ctx_;
  std::vector<LocalSymbolTable*> scope_stack_;
};

}  // namespace

SymbolTable BuildSymbolTable(
    const Program& program,
    StatementNumerizer numerizer,
    DebugCtx& debug_ctx) {
  SymbolTableBuilder builder(numerizer, debug_ctx);
  SymbolTable symbol_table = builder.Build(program);
  symbol_table.SetStatementNumerizer(std::move(numerizer));
  return symbol_table;
}

SymbolTable BuildSymbolTable(
    const Program& program,
    StatementNumerizer numerizer) {
  DebugCtx debug_ctx;
  SymbolTable symbol_table = BuildSymbolTable(
      program,
      std::move(numerizer),
      debug_ctx);
  if (debug_ctx.GetErrors().HasErrors()) {
    debug_ctx.GetErrors().ThrowErrors();
  }

  return symbol_table;
}

SymbolTable BuildSymbolTable(const Program& program, DebugCtx& debug_ctx) {
  StatementNumerizer numerizer = BuildStatementNumerizer(program);
  return BuildSymbolTable(program, std::move(numerizer), debug_ctx);
}

SymbolTable BuildSymbolTable(const Program& program) {
  DebugCtx debug_ctx;
  SymbolTable symbol_table = BuildSymbolTable(program, debug_ctx);
  if (debug_ctx.GetErrors().HasErrors()) {
    debug_ctx.GetErrors().ThrowErrors();
  }

  return symbol_table;
}

}  // namespace Front
