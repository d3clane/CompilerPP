#include "SemanticAnalysis/SymbolTable.hpp"

#include <algorithm>
#include <cassert>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <variant>

#include "Debug/DebugCtx.hpp"
#include "SemanticAnalysis/TypeDefiner.hpp"
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

LocalSymbolTable::ErrorMsg LocalSymbolTable::AddSymbolInfo(SymbolData symbol_data) {
  if (GetSymbolInfoInLocalScope(symbol_data.name) != nullptr) {
    return "Duplicate declaration in the same scope: " + symbol_data.name;
  }

  const SymbolData* visible_parent_symbol = GetVisibleSymbolInParents(symbol_data.name);
  if (visible_parent_symbol != nullptr &&
      !SymbolData::CanBeShadowed(*visible_parent_symbol)) {
    return "Symbol cannot be shadowed: " + symbol_data.name;
  }

  symbols_.emplace(symbol_data.name, std::move(symbol_data));
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
  owner_by_table_[&table] = node;
}

const LocalSymbolTable* SymbolTable::GetTable(const ASTNode* node) const {
  const auto table_it = table_by_node_.find(node);
  if (table_it == table_by_node_.end()) {
    return nullptr;
  }

  return table_it->second;
}

const ASTNode* SymbolTable::GetScopeOwner(const LocalSymbolTable* table) const {
  const auto owner_it = owner_by_table_.find(table);
  if (owner_it == owner_by_table_.end()) {
    return nullptr;
  }

  return owner_it->second;
}

void SymbolTable::SetStatementNumerizer(StatementNumerizer numerizer) {
  statement_numerizer_ =
      std::make_unique<StatementNumerizer>(std::move(numerizer));
}

const StatementNumerizer* SymbolTable::GetStatementNumerizer() const {
  return statement_numerizer_.get();
}

namespace {

class ClassSymbolTableInitter {
 public:
  ClassSymbolTableInitter(
      const ClassDeclarationStatement& class_declaration,
      const TypeDefiner& type_definer,
      const StatementNumerizer& numerizer,
      DebugCtx& debug_ctx,
      LocalSymbolTable& class_scope)
      : class_declaration_(class_declaration),
        type_definer_(type_definer),
        numerizer_(numerizer),
        debug_ctx_(debug_ctx),
        class_scope_(class_scope) {}

  void Init() {
    const std::optional<std::vector<const ClassDeclarationStatement*>> base_chain =
        BuildBaseClassChain();
    if (!base_chain.has_value()) {
      return;
    }

    for (size_t i = 0; i < base_chain->size(); ++i) {
      const ClassDeclarationStatement* base_class = (*base_chain)[i];
      assert(base_class != nullptr);

      for (size_t field_i = 0; field_i < base_class->fields.size(); ++field_i) {
        AddInheritedField(base_class->fields[field_i]);
      }

      for (size_t method_i = 0; method_i < base_class->methods.size(); ++method_i) {
        AddInheritedMethod(base_class->methods[method_i]);
      }
    }
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

  SymbolData BuildFieldSymbolData(const DeclarationStatement& field) const {
    return SymbolData{
        field.type,
        field.variable_name,
        field.is_mutable,
        SymbolDebugInfo{"variable declaration"},
        &field,
        GetNodeRefOrThrow(&field),
        SymbolKind::Variable};
  }

  SymbolData BuildMethodSymbolData(const FunctionDeclarationStatement& method) const {
    return SymbolData{
        Type{BuildFunctionType(method)},
        method.function_name,
        false,
        SymbolDebugInfo{"function declaration"},
        &method,
        GetNodeRefOrThrow(&method),
        SymbolKind::Function};
  }

  void AddInheritedField(const DeclarationStatement& field) {
    const LocalSymbolTable::ErrorMsg error_msg =
        class_scope_.AddSymbolInfo(BuildFieldSymbolData(field));
    if (!error_msg.empty()) {
      debug_ctx_.GetErrors().AddError(&field, error_msg);
    }
  }

  void AddInheritedMethod(const FunctionDeclarationStatement& method) {
    const SymbolData* existing_symbol =
        class_scope_.GetSymbolInfoInLocalScope(method.function_name);
    if (existing_symbol != nullptr) {
      if (existing_symbol->kind == SymbolKind::Function) {
        return;
      }

      debug_ctx_.GetErrors().AddError(
          &method,
          "Duplicate inherited member in class scope: " + method.function_name);
      return;
    }

    const LocalSymbolTable::ErrorMsg error_msg =
        class_scope_.AddSymbolInfo(BuildMethodSymbolData(method));
    if (!error_msg.empty()) {
      debug_ctx_.GetErrors().AddError(&method, error_msg);
    }
  }

  std::optional<std::vector<const ClassDeclarationStatement*>> BuildBaseClassChain() const {
    std::set<std::string> visited_classes{class_declaration_.class_name};
    std::vector<const ClassDeclarationStatement*> chain;

    const ClassDeclarationStatement* current_class = &class_declaration_;
    while (current_class->base_class_name.has_value()) {
      const ClassDeclarationStatement* base_class =
          type_definer_.GetClassDeclaration(*current_class->base_class_name);
      if (base_class == nullptr) {
        return chain;
      }

      if (!visited_classes.insert(base_class->class_name).second) {
        debug_ctx_.GetErrors().AddError(
            &class_declaration_,
            "Cyclic class inheritance detected");
        return std::nullopt;
      }

      chain.push_back(base_class);
      current_class = base_class;
    }

    std::reverse(chain.begin(), chain.end());
    return chain;
  }

  const ClassDeclarationStatement& class_declaration_;
  const TypeDefiner& type_definer_;
  const StatementNumerizer& numerizer_;
  DebugCtx& debug_ctx_;
  LocalSymbolTable& class_scope_;
};

class SymbolTableBuilder {
 public:
  SymbolTableBuilder(
      const TypeDefiner& type_definer,
      const StatementNumerizer& numerizer,
      DebugCtx& debug_ctx)
      : type_definer_(type_definer),
        numerizer_(numerizer),
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

  LocalSymbolTable& EnterScope(const ASTNode* owner_node) {
    LocalSymbolTable* parent = nullptr;
    if (!scope_stack_.empty()) {
      parent = scope_stack_.back();
    }

    LocalSymbolTable& table = symbol_table_.CreateLocalTable(parent);
    scope_stack_.push_back(&table);
    symbol_table_.AddTable(owner_node, table);
    return table;
  }

  void LeaveScope() {
    assert(!scope_stack_.empty());
    scope_stack_.pop_back();
  }

  void RegisterNode(const ASTNode* node) {
    symbol_table_.AddTable(node, CurrentScope());
  }

  void TryAddSymbolInfo(const ASTNode* node, SymbolData symbol_data) {
    const LocalSymbolTable::ErrorMsg error_msg =
        CurrentScope().AddSymbolInfo(std::move(symbol_data));
    if (!error_msg.empty()) {
      debug_ctx_.GetErrors().AddError(node, error_msg);
    }
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

    TryAddSymbolInfo(
        &declaration,
        SymbolData{
            declaration.type,
            declaration.variable_name,
            declaration.is_mutable,
            SymbolDebugInfo{"variable declaration"},
            &declaration,
            GetNodeRefOrThrow(&declaration),
            SymbolKind::Variable});
  }

  void VisitFunctionDeclarationStatement(
      const FunctionDeclarationStatement& function_declaration) {
    RegisterNode(&function_declaration);
    TryAddSymbolInfo(
        &function_declaration,
        SymbolData{
            Type{BuildFunctionType(function_declaration)},
            function_declaration.function_name,
            false,
            SymbolDebugInfo{"function declaration"},
            &function_declaration,
            GetNodeRefOrThrow(&function_declaration),
            SymbolKind::Function});
    VisitFunctionBody(function_declaration);
  }

  void VisitFunctionBody(const FunctionDeclarationStatement& function_declaration) {
    assert(function_declaration.body != nullptr);

    EnterScope(function_declaration.body.get());

    for (size_t i = 0; i < function_declaration.parameters.size(); ++i) {
      RegisterNode(&function_declaration.parameters[i]);
      TryAddSymbolInfo(
          &function_declaration.parameters[i],
          SymbolData{
              function_declaration.parameters[i].type,
              function_declaration.parameters[i].name,
              false,
              SymbolDebugInfo{"function parameter"},
              &function_declaration.parameters[i],
              GetNodeRefOrThrow(&function_declaration.parameters[i]),
              SymbolKind::Parameter});
    }

    VisitStatementsInCurrentScope(function_declaration.body->statements);
    LeaveScope();
  }

  void VisitClassDeclarationStatement(
      const ClassDeclarationStatement& class_declaration) {
    EnterScope(&class_declaration);
    ClassSymbolTableInitter class_table_initter(
        class_declaration,
        type_definer_,
        numerizer_,
        debug_ctx_,
        CurrentScope());
    class_table_initter.Init();

    for (size_t i = 0; i < class_declaration.fields.size(); ++i) {
      VisitDeclarationStatement(class_declaration.fields[i]);
    }

    for (size_t i = 0; i < class_declaration.methods.size(); ++i) {
      VisitClassMethodDeclaration(class_declaration.methods[i], class_declaration);
    }

    LeaveScope();
  }

  bool IsMethodDeclaredInClass(
      const ASTNode* declaration_node,
      const ClassDeclarationStatement& class_declaration) const {
    for (size_t i = 0; i < class_declaration.methods.size(); ++i) {
      if (&class_declaration.methods[i] == declaration_node) {
        return true;
      }
    }

    return false;
  }

  void VisitClassMethodDeclaration(
      const FunctionDeclarationStatement& method_declaration,
      const ClassDeclarationStatement& owner_class) {
    auto& declared_method_names = declared_method_names_by_class_[&owner_class];
    if (!declared_method_names.insert(method_declaration.function_name).second) {
      debug_ctx_.GetErrors().AddError(
          &method_declaration,
          "Duplicate declaration in the same scope: " + method_declaration.function_name);
      RegisterNode(&method_declaration);
      VisitFunctionBody(method_declaration);
      return;
    }

    const SymbolData* existing_symbol =
        CurrentScope().GetSymbolInfoInLocalScope(method_declaration.function_name);
    if (existing_symbol != nullptr &&
        existing_symbol->kind == SymbolKind::Function &&
        !IsMethodDeclaredInClass(existing_symbol->declaration_node, owner_class)) {
      RegisterNode(&method_declaration);
      VisitFunctionBody(method_declaration);
      return;
    }

    VisitFunctionDeclarationStatement(method_declaration);
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

  const TypeDefiner& type_definer_;
  const StatementNumerizer& numerizer_;
  SymbolTable symbol_table_;
  DebugCtx& debug_ctx_;
  std::vector<LocalSymbolTable*> scope_stack_;
  std::map<const ClassDeclarationStatement*, std::set<std::string>> declared_method_names_by_class_;
};

}  // namespace

SymbolTable BuildSymbolTable(
    const Program& program,
    const TypeDefiner& type_definer,
    StatementNumerizer numerizer,
    DebugCtx& debug_ctx) {
  SymbolTableBuilder builder(type_definer, numerizer, debug_ctx);
  SymbolTable symbol_table = builder.Build(program);
  symbol_table.SetStatementNumerizer(std::move(numerizer));
  return symbol_table;
}

SymbolTable BuildSymbolTable(
    const Program& program,
    const TypeDefiner& type_definer,
    StatementNumerizer numerizer) {
  DebugCtx debug_ctx;
  SymbolTable symbol_table = BuildSymbolTable(
      program,
      type_definer,
      std::move(numerizer),
      debug_ctx);
  if (debug_ctx.GetErrors().HasErrors()) {
    debug_ctx.GetErrors().ThrowErrors();
  }

  return symbol_table;
}

SymbolTable BuildSymbolTable(
    const Program& program,
    const TypeDefiner& type_definer,
    DebugCtx& debug_ctx) {
  StatementNumerizer numerizer = BuildStatementNumerizer(program);
  return BuildSymbolTable(program, type_definer, std::move(numerizer), debug_ctx);
}

SymbolTable BuildSymbolTable(
    const Program& program,
    const TypeDefiner& type_definer) {
  DebugCtx debug_ctx;
  SymbolTable symbol_table = BuildSymbolTable(program, type_definer, debug_ctx);
  if (debug_ctx.GetErrors().HasErrors()) {
    debug_ctx.GetErrors().ThrowErrors();
  }

  return symbol_table;
}

SymbolTable BuildSymbolTable(
    const Program& program,
    StatementNumerizer numerizer) {
  const TypeDefiner type_definer = BuildTypeDefiner(program);
  return BuildSymbolTable(program, type_definer, std::move(numerizer));
}

SymbolTable BuildSymbolTable(
    const Program& program,
    StatementNumerizer numerizer,
    DebugCtx& debug_ctx) {
  const TypeDefiner type_definer = BuildTypeDefiner(program);
  return BuildSymbolTable(program, type_definer, std::move(numerizer), debug_ctx);
}

SymbolTable BuildSymbolTable(const Program& program, DebugCtx& debug_ctx) {
  const TypeDefiner type_definer = BuildTypeDefiner(program);
  return BuildSymbolTable(program, type_definer, debug_ctx);
}

SymbolTable BuildSymbolTable(const Program& program) {
  DebugCtx debug_ctx;
  SymbolTable symbol_table = BuildSymbolTable(program, debug_ctx);
  if (debug_ctx.GetErrors().HasErrors()) {
    debug_ctx.GetErrors().ThrowErrors();
  }

  return symbol_table;
}

}  // namespace Parsing
