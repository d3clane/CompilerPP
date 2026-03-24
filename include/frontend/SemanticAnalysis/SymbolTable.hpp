#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Parsing/Ast.hpp"

namespace Parsing {

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
  AstNodeID declaration_node_id;
  size_t in_scope_stmt_id;
  SymbolKind kind;

  static bool CanBeShadowed(const SymbolData& symbol_data) {
    return symbol_data.kind != SymbolKind::Function;
  }

  friend bool operator==(const SymbolData& left, const SymbolData& right) = default;
};

class LocalSymbolTable {
 public:
  explicit LocalSymbolTable(LocalSymbolTable* parent);

  const SymbolData* GetSymbolInfo(const std::string& name) const;
  const SymbolData* GetSymbolInfoInLocalScope(const std::string& name) const;
  void AddSymbolInfo(SymbolData symbol_data);
  LocalSymbolTable* GetParent() const;

 private:
  const SymbolData* GetVisibleSymbolInParents(const std::string& name) const;

  LocalSymbolTable* parent_;
  std::map<std::string, SymbolData> symbols_;
};

class SymbolTable {
 public:
  LocalSymbolTable& CreateLocalTable(LocalSymbolTable* parent);

  void AddTable(AstNodeID node_id, LocalSymbolTable& table);
  void AddTable(const ASTNode& node, LocalSymbolTable& table);

  const LocalSymbolTable* GetTable(AstNodeID node_id) const;
  const LocalSymbolTable* GetTable(const ASTNode& node) const;

  const SymbolData* GetSymbolInfoInLocalScope(
      const std::string& name,
      AstNodeID node_id) const;
  const SymbolData* GetSymbolInfoInLocalScope(
      const std::string& name,
      const ASTNode& node) const;

  const SymbolData* GetSymbolInfo(const std::string& name, AstNodeID node_id) const;
  const SymbolData* GetSymbolInfo(const std::string& name, const ASTNode& node) const;

 private:
  std::unordered_map<AstNodeID, LocalSymbolTable*> table_by_node_id_;
  std::vector<std::unique_ptr<LocalSymbolTable>> owned_tables_;
};

SymbolTable BuildSymbolTable(const Program& program);

}  // namespace Parsing
