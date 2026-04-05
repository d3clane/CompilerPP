#include "SemanticAnalysis/Resolver.hpp"

#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

#include "Debug/DebugCtx.hpp"
#include "SemanticAnalysis/SymbolTable.hpp"
#include "SemanticAnalysis/TypeDefiner.hpp"
#include "Utils/ClassMemberLookup.hpp"
#include "Utils/Overload.hpp"

namespace Parsing {

namespace {

class UseResolverBuilder {
 public:
  enum class ClassFallbackMode {
    None,
    FunctionsFromBaseClasses,
  };

  UseResolverBuilder(
      const Program& program,
      const SymbolTable& symbol_table,
      const TypeDefiner& type_definer,
      DebugCtx& debug_ctx)
      : program_(program),
        symbol_table_(symbol_table),
        type_definer_(type_definer),
        debug_ctx_(debug_ctx) {}

  std::map<UseResolver::Use, UseResolver::ResolvedSymbol> Build() {
    Init();
    VisitStatements(program_.top_statements);
    return use_to_resolved_symbol_;
  }

 private:
  void Init() {
    if (initialized_) {
      return;
    }

    global_scope_ = symbol_table_.GetTable(&program_);
    if (global_scope_ == nullptr) {
      throw std::runtime_error("symbol table is incorrect for this program");
    }

    numerizer_ = symbol_table_.GetStatementNumerizer();
    if (numerizer_ == nullptr) {
      throw std::runtime_error("symbol table is incorrect for this program");
    }

    for (const auto& [_, class_declaration] :
         type_definer_.GetClassDeclarations()) {
      if (class_declaration == nullptr) {
        continue;
      }

      const LocalSymbolTable* class_scope =
          symbol_table_.GetTable(class_declaration);
      if (class_scope == nullptr) {
        throw std::runtime_error("symbol table is incorrect for this program");
      }

      class_declaration_by_scope_[class_scope] = class_declaration;
    }

    initialized_ = true;
  }
  using ScopedStmtRef = StatementNumerizer::ScopedStmtRef;

  ScopedStmtRef GetNodeRefOrThrow(const ASTNode* node) const {
    const std::optional<ScopedStmtRef> ref = numerizer_->GetRef(node);
    if (!ref.has_value()) {
      throw std::runtime_error("statement numerizer is incorrect for this program");
    }

    return *ref;
  }

  bool IsFunctionForwardUseAllowed(const ASTNode* declaration_scope_owner) const {
    const LocalSymbolTable* declaration_scope =
        symbol_table_.GetTable(declaration_scope_owner);
    if (declaration_scope == nullptr) {
      throw std::runtime_error("symbol table is incorrect for this program");
    }

    return declaration_scope == global_scope_ ||
           class_declaration_by_scope_.contains(declaration_scope);
  }

  bool IsClassScopeOwner(const ASTNode* scope_owner) const {
    const LocalSymbolTable* scope = symbol_table_.GetTable(scope_owner);
    if (scope == nullptr) {
      throw std::runtime_error("symbol table is incorrect for this program");
    }

    return class_declaration_by_scope_.contains(scope);
  }

  const ClassDeclarationStatement* GetClassDeclarationForScope(
      const LocalSymbolTable* scope) const {
    const auto class_it = class_declaration_by_scope_.find(scope);
    if (class_it == class_declaration_by_scope_.end()) {
      return nullptr;
    }

    return class_it->second;
  }

  UseResolver::ResolvedSymbol BuildResolvedSymbol(
      const SymbolData& symbol_data,
      const ClassDeclarationStatement* declaring_class) const {
    return UseResolver::ResolvedSymbol{
        symbol_data.declaration_node,
        symbol_data,
        declaring_class};
  }

  UseResolver::ResolvedSymbol BuildResolvedSymbol(
      const SymbolData& symbol_data,
      const LocalSymbolTable* declaring_scope) const {
    return BuildResolvedSymbol(
        symbol_data,
        GetClassDeclarationForScope(declaring_scope));
  }

  bool IsSymbolVisible(
      const SymbolData& symbol_data,
      const ScopedStmtRef& use_ref) const {
    if (symbol_data.declaration_ref.stmt_id_in_scope == 0) {
      return true;
    }

    const ASTNode* declaration_scope_owner =
        numerizer_->GetScopeOwnerFromRef(symbol_data.declaration_ref);
    if (declaration_scope_owner == nullptr) {
      throw std::runtime_error("statement numerizer is incorrect for this program");
    }

    if (symbol_data.kind == SymbolKind::Function &&
        IsFunctionForwardUseAllowed(declaration_scope_owner)) {
      return true;
    }

    if (symbol_data.kind == SymbolKind::Variable &&
        IsClassScopeOwner(declaration_scope_owner)) {
      // Class fields are treated as visible from any method of that class.
      return true;
    }

    const std::optional<ScopedStmtRef> projected_use_ref =
        numerizer_->ProjectUseToScope(use_ref, declaration_scope_owner);
    if (!projected_use_ref.has_value()) {
      throw std::runtime_error("statement numerizer is incorrect for this program");
    }

    return symbol_data.declaration_ref.stmt_id_in_scope <
           projected_use_ref->stmt_id_in_scope;
  }

  std::optional<UseResolver::ResolvedSymbol> ResolveUse(
      const std::string& name,
      const ASTNode* use_node,
      ClassFallbackMode class_fallback_mode) {
    const LocalSymbolTable* scope = symbol_table_.GetTable(use_node);
    if (scope == nullptr) {
      throw std::runtime_error("symbol table is incorrect for this program");
    }

    const ScopedStmtRef use_ref = GetNodeRefOrThrow(use_node);
    for (const LocalSymbolTable* local_scope = scope;
         local_scope != nullptr;
         local_scope = local_scope->GetParent()) {
      const SymbolData* local_symbol =
          local_scope->GetSymbolInfoInLocalScope(name);
      if (local_symbol != nullptr &&
          IsSymbolVisible(*local_symbol, use_ref)) {
        return BuildResolvedSymbol(*local_symbol, local_scope);
      }

      const ClassDeclarationStatement* current_class =
          GetClassDeclarationForScope(local_scope);
      if (current_class == nullptr ||
          class_fallback_mode == ClassFallbackMode::None) {
        continue;
      }

      const std::optional<ClassMemberLookupResult> lookup_result = LookupClassMember(
          *current_class,
          name,
          ClassMemberSearchMode::CurrentClassAndBases,
          symbol_table_,
          type_definer_);
      if (lookup_result.has_value()) {
        assert(lookup_result->symbol_data != nullptr);
        assert(lookup_result->declaring_class != nullptr);
        return BuildResolvedSymbol(
            *lookup_result->symbol_data,
            lookup_result->declaring_class);
      }
    }

    debug_ctx_.GetErrors().AddError(use_node, "use before def: " + name);
    return std::nullopt;
  }

  const UseResolver::ResolvedSymbol* RegisterUse(
      const ASTNode* use_node,
      const std::string& name,
      ClassFallbackMode class_fallback_mode = ClassFallbackMode::None) {
    const std::optional<UseResolver::ResolvedSymbol> resolved_symbol =
        ResolveUse(name, use_node, class_fallback_mode);
    if (!resolved_symbol.has_value()) {
      return nullptr;
    }

    const auto [resolved_it, _] = use_to_resolved_symbol_.emplace(
        UseResolver::Use{use_node, name},
        *resolved_symbol);
    return &resolved_it->second;
  }

  const ClassDeclarationStatement* ResolveReceiverClassDeclaration(
      const std::string& receiver_name,
      const SymbolData& receiver_symbol_data,
      const ASTNode* use_node) {
    const auto* receiver_class_type =
        std::get_if<ClassType>(&receiver_symbol_data.type.type);
    if (receiver_class_type == nullptr) {
      debug_ctx_.GetErrors().AddError(
          use_node,
          "member access receiver is not a class object: " + receiver_name);
      return nullptr;
    }

    const ClassDeclarationStatement* class_declaration =
        type_definer_.GetClassDeclaration(receiver_class_type->class_name);
    if (class_declaration == nullptr) {
      debug_ctx_.GetErrors().AddError(
          use_node,
          "unknown class type in member access: " + receiver_class_type->class_name);
      return nullptr;
    }

    return class_declaration;
  }

  std::optional<UseResolver::ResolvedSymbol> ResolveClassMember(
      const ClassDeclarationStatement& class_declaration,
      const std::string& member_name,
      ClassMemberSearchMode search_mode) {
    const std::optional<ClassMemberLookupResult> lookup_result = LookupClassMember(
        class_declaration,
        member_name,
        search_mode,
        symbol_table_,
        type_definer_);
    if (!lookup_result.has_value()) {
      return UseResolver::ResolvedSymbol{};
    }

    assert(lookup_result->symbol_data != nullptr);
    assert(lookup_result->declaring_class != nullptr);
    return BuildResolvedSymbol(
        *lookup_result->symbol_data,
        lookup_result->declaring_class);
  }

  void ResolveMethodCallMember(
      const MethodCall& method_call,
      const SymbolData& receiver_symbol_data) {
    const ClassDeclarationStatement* class_declaration =
        ResolveReceiverClassDeclaration(
            method_call.object_name.name,
            receiver_symbol_data,
            &method_call);
    if (class_declaration == nullptr) {
      return;
    }

    const std::optional<UseResolver::ResolvedSymbol> resolved_method =
        ResolveClassMember(
            *class_declaration,
            method_call.function_call.function_name,
            ClassMemberSearchMode::CurrentClassAndBases);
    if (!resolved_method.has_value()) {
      return;
    }

    if (resolved_method->definition_node == nullptr ||
        resolved_method->symbol_data.kind != SymbolKind::Function) {
      debug_ctx_.GetErrors().AddError(
          &method_call,
          "unknown method " + method_call.function_call.function_name +
              " for class " + class_declaration->class_name);
      return;
    }

    use_to_resolved_symbol_.insert_or_assign(
        UseResolver::Use{
            &method_call.function_call,
            method_call.function_call.function_name},
        *resolved_method);
  }

  void ResolveFieldAccessMember(
      const FieldAccess& field_access,
      const SymbolData& receiver_symbol_data) {
    const ClassDeclarationStatement* class_declaration =
        ResolveReceiverClassDeclaration(
            field_access.object_name.name,
            receiver_symbol_data,
            &field_access);
    if (class_declaration == nullptr) {
      return;
    }

    const std::optional<UseResolver::ResolvedSymbol> resolved_field =
        ResolveClassMember(
            *class_declaration,
            field_access.field_name.name,
            ClassMemberSearchMode::CurrentClassOnly);
    if (!resolved_field.has_value()) {
      return;
    }

    if (resolved_field->definition_node == nullptr ||
        resolved_field->symbol_data.kind != SymbolKind::Variable) {
      debug_ctx_.GetErrors().AddError(
          &field_access,
          "unknown field " + field_access.field_name.name +
              " for class " + class_declaration->class_name);
      return;
    }

    use_to_resolved_symbol_.insert_or_assign(
        UseResolver::Use{&field_access.field_name, field_access.field_name.name},
        *resolved_field);
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
    if (declaration.initializer != nullptr) {
      VisitExpression(*declaration.initializer);
    }
  }

  void VisitFunctionDeclaration(const FunctionDeclarationStatement& function_declaration) {
    assert(function_declaration.body != nullptr);
    VisitStatements(function_declaration.body->statements);
  }

  void VisitClassDeclaration(const ClassDeclarationStatement& class_declaration) {
    for (size_t i = 0; i < class_declaration.fields.size(); ++i) {
      VisitDeclarationStatement(class_declaration.fields[i]);
    }

    for (size_t i = 0; i < class_declaration.methods.size(); ++i) {
      VisitFunctionDeclaration(class_declaration.methods[i]);
    }
  }

  void VisitAssignmentStatement(const AssignmentStatement& assignment) {
    RegisterUse(&assignment, assignment.variable_name);

    assert(assignment.expr != nullptr);
    VisitExpression(*assignment.expr);
  }

  void VisitPrintStatement(const PrintStatement& print_statement) {
    assert(print_statement.expr != nullptr);
    VisitExpression(*print_statement.expr);
  }

  void VisitIfStatement(const IfStatement& if_statement) {
    assert(if_statement.condition != nullptr);
    assert(if_statement.true_block != nullptr);
    assert(if_statement.else_tail != nullptr);

    VisitExpression(*if_statement.condition);
    VisitBlock(*if_statement.true_block);
    VisitElseTail(*if_statement.else_tail);
  }

  void VisitElseTail(const ElseTail& else_tail) {
    if (else_tail.else_if != nullptr) {
      VisitIfStatement(*else_tail.else_if);
      return;
    }

    if (else_tail.else_block != nullptr) {
      VisitBlock(*else_tail.else_block);
    }
  }

  void VisitReturnStatement(const ReturnStatement& return_statement) {
    if (return_statement.expr != nullptr) {
      VisitExpression(*return_statement.expr);
    }
  }

  void VisitBlock(const Block& block) {
    VisitStatements(block.statements);
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
            [this](const IdentifierExpression& identifier_expression) {
              RegisterUse(&identifier_expression, identifier_expression.name);
            },
            [](const LiteralExpression&) {
            },
            [this](const FunctionCall& function_call) {
              const UseResolver::ResolvedSymbol* resolved_symbol =
                  RegisterUse(
                      &function_call,
                      function_call.function_name,
                      ClassFallbackMode::FunctionsFromBaseClasses);
              if (resolved_symbol != nullptr &&
                  resolved_symbol->symbol_data.kind != SymbolKind::Function) {
                debug_ctx_.GetErrors().AddError(
                    &function_call,
                    "called object is not a function: " + function_call.function_name);
              }

              for (size_t i = 0; i < function_call.arguments.size(); ++i) {
                assert(function_call.arguments[i] != nullptr);
                VisitCallArgument(*function_call.arguments[i]);
              }
            },
            [this](const FieldAccess& field_access) {
              const UseResolver::ResolvedSymbol* resolved_receiver =
                  RegisterUse(&field_access.object_name, field_access.object_name.name);
              if (resolved_receiver != nullptr) {
                ResolveFieldAccessMember(field_access, resolved_receiver->symbol_data);
              }
            },
            [this](const MethodCall& method_call) {
              const UseResolver::ResolvedSymbol* resolved_receiver =
                  RegisterUse(&method_call.object_name, method_call.object_name.name);
              if (resolved_receiver != nullptr) {
                ResolveMethodCallMember(method_call, resolved_receiver->symbol_data);
              }

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
  const SymbolTable& symbol_table_;
  const StatementNumerizer* numerizer_ = nullptr;
  const TypeDefiner& type_definer_;
  DebugCtx& debug_ctx_;
  bool initialized_ = false;
  const LocalSymbolTable* global_scope_ = nullptr;
  std::map<const LocalSymbolTable*, const ClassDeclarationStatement*>
      class_declaration_by_scope_;
  std::map<UseResolver::Use, UseResolver::ResolvedSymbol> use_to_resolved_symbol_;
};

}  // namespace

const UseResolver::ResolvedSymbol* UseResolver::GetResolvedSymbol(
    const std::string& name,
    const ASTNode* curr_node) const {
  if (curr_node == nullptr) {
    return nullptr;
  }

  const auto resolved_it =
      use_to_resolved_symbol_.find(Use{curr_node, std::string(name)});
  if (resolved_it == use_to_resolved_symbol_.end()) {
    return nullptr;
  }

  return &resolved_it->second;
}

const ASTNode* UseResolver::GetUsedVarDef(
    const std::string& name,
    const ASTNode* curr_node) const {
  const ResolvedSymbol* resolved_symbol = GetResolvedSymbol(name, curr_node);
  if (resolved_symbol == nullptr) {
    return nullptr;
  }

  return resolved_symbol->definition_node;
}

UseResolver BuildUseResolver(
    const Program& program,
    SymbolTable& symbol_table,
    DebugCtx& debug_ctx) {
  if (symbol_table.GetTable(&program) == nullptr) {
    throw std::runtime_error(
        "BuildUseResolver requires a symbol table built for the same program");
  }

  if (symbol_table.GetStatementNumerizer() == nullptr) {
    throw std::runtime_error(
        "BuildUseResolver requires a symbol table with statement numerizer");
  }

  const TypeDefiner type_definer = BuildTypeDefiner(program);
  UseResolver resolver;
  UseResolverBuilder builder(
      program,
      symbol_table,
      type_definer,
      debug_ctx);
  resolver.use_to_resolved_symbol_ = builder.Build();

  return resolver;
}

UseResolver BuildUseResolver(const Program& program, SymbolTable& symbol_table) {
  DebugCtx debug_ctx;
  UseResolver resolver = BuildUseResolver(program, symbol_table, debug_ctx);
  if (debug_ctx.GetErrors().HasErrors()) {
    debug_ctx.GetErrors().ThrowErrors();
  }

  return resolver;
}

}  // namespace Parsing
