#pragma once

#include <cstddef>
#include <map>
#include <optional>

#include "Parsing/Ast.hpp"

namespace Parsing {

class StatementNumerizer {
 public:
  struct ScopedStmtRef {
    const ASTNode* node = nullptr;
    size_t stmt_id_in_scope = 0;
    const ASTNode* parent_node = nullptr;

    friend bool operator==(const ScopedStmtRef& left, const ScopedStmtRef& right) = default;
  };

  std::optional<ScopedStmtRef> GetRef(const ASTNode* node) const;
  const ASTNode* GetScopeOwnerFromRef(const ScopedStmtRef& ref) const;
  std::optional<ScopedStmtRef> ProjectUseToScope(
      ScopedStmtRef use_ref,
      const ASTNode* target_scope_owner) const;

 private:
  void BindNode(const ASTNode* node, const ScopedStmtRef& ref);

  std::map<const ASTNode*, ScopedStmtRef> ref_by_node_;

  friend class StatementNumerizerBuilder;
};

StatementNumerizer BuildStatementNumerizer(const Program& program);

}  // namespace Parsing
