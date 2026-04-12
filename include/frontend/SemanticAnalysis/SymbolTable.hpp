#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Parsing/Ast.hpp"
#include "SemanticAnalysis/StatementNumerizer.hpp"

namespace Parsing {

class DebugCtx;

enum class SymbolKind {
  Variable,
  Function,
  Class,
  Parameter,
};

struct SymbolData {
  const Type* type = nullptr;
  StatementNumerizer::ScopedStmtRef declaration_ref;
  SymbolKind kind;

  const ASTNode* GetDeclarationNode() const;

  static SymbolData CreateVariableSymbolData(
      const DeclarationStatement& declaration,
      StatementNumerizer::ScopedStmtRef declaration_ref);
  static SymbolData CreateFunctionSymbolData(
      const FunctionDeclarationStatement& function_declaration,
      StatementNumerizer::ScopedStmtRef declaration_ref);
  static SymbolData CreateParameterSymbolData(
      const FunctionParameter& parameter,
      StatementNumerizer::ScopedStmtRef declaration_ref);

  static bool CanBeShadowed(const SymbolData& symbol_data) {
    return symbol_data.kind != SymbolKind::Function &&
           symbol_data.kind != SymbolKind::Class;
  }

  friend bool operator==(const SymbolData& left, const SymbolData& right) = default;
};

class LocalSymbolTable {
 public:
  using ErrorMsg = std::string;

  explicit LocalSymbolTable(LocalSymbolTable* parent);

  const SymbolData* GetSymbolInfoInLocalScope(const std::string& name) const;
  ErrorMsg AddSymbolInfo(const std::string& name, SymbolData symbol_data);
  LocalSymbolTable* GetParent() const;

 private:
  const SymbolData* GetVisibleSymbolInParents(const std::string& name) const;

  LocalSymbolTable* parent_;
  std::map<std::string, SymbolData> symbols_;
};

class SymbolTable {
 public:
  LocalSymbolTable& CreateLocalTable(LocalSymbolTable* parent);

  void AddTable(const ASTNode* node, LocalSymbolTable& table);
  void SetScopeOwner(const ASTNode* owner, LocalSymbolTable& table);

  const LocalSymbolTable* GetTable(const ASTNode* node) const;
  const ASTNode* GetScopeOwner(const LocalSymbolTable* table) const;
  const ClassDeclarationStatement* GetIfClassDeclarationOwner(
      const LocalSymbolTable* table) const;
  const FunctionDeclarationStatement* GetIfFunctionDeclarationOwner(
      const LocalSymbolTable* table) const;
  void SetStatementNumerizer(StatementNumerizer numerizer);
  const StatementNumerizer* GetStatementNumerizer() const;

 private:
  std::map<const ASTNode*, LocalSymbolTable*> table_by_node_;
  std::map<const LocalSymbolTable*, const ASTNode*> scope_owner_by_table_;
  std::vector<std::unique_ptr<LocalSymbolTable>> owned_tables_;
  std::unique_ptr<StatementNumerizer> statement_numerizer_;
};

SymbolTable BuildSymbolTable(
    const Program& program,
    StatementNumerizer numerizer,
    DebugCtx& debug_ctx);
SymbolTable BuildSymbolTable(
    const Program& program,
    StatementNumerizer numerizer);
SymbolTable BuildSymbolTable(const Program& program, DebugCtx& debug_ctx);
SymbolTable BuildSymbolTable(const Program& program);

}  // namespace Parsing
