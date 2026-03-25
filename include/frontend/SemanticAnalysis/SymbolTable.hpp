#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Parsing/Ast.hpp"

namespace Parsing {

class DebugCtx;

struct SymbolDebugInfo {
  std::string context;

  friend bool operator==(const SymbolDebugInfo& left, const SymbolDebugInfo& right) = default;
};

enum class SymbolKind {
  Variable,
  Function,
  Parameter,
};

struct SymbolData {
  Type type;
  std::string name;
  bool is_mutable;
  SymbolDebugInfo debug_info;
  const ASTNode* declaration_node;
  size_t in_scope_stmt_id;
  SymbolKind kind;

  static bool CanBeShadowed(const SymbolData& symbol_data) {
    return symbol_data.kind != SymbolKind::Function;
  }

  friend bool operator==(const SymbolData& left, const SymbolData& right) = default;
};

class LocalSymbolTable {
 public:
  using ErrorMsg = std::string;

  explicit LocalSymbolTable(LocalSymbolTable* parent);

  const SymbolData* GetSymbolInfo(const std::string& name) const;
  const SymbolData* GetSymbolInfoInLocalScope(const std::string& name) const;
  ErrorMsg AddSymbolInfo(SymbolData symbol_data);
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

  const LocalSymbolTable* GetTable(const ASTNode* node) const;

  const SymbolData* GetSymbolInfoInLocalScope(
      const std::string& name,
      const ASTNode* node) const;

  const SymbolData* GetSymbolInfo(const std::string& name, const ASTNode* node) const;

 private:
  std::map<const ASTNode*, LocalSymbolTable*> table_by_node_;
  std::vector<std::unique_ptr<LocalSymbolTable>> owned_tables_;
};

SymbolTable BuildSymbolTable(const Program& program, DebugCtx& debug_ctx);
SymbolTable BuildSymbolTable(const Program& program);

}  // namespace Parsing
