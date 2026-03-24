#pragma once

#include <functional>
#include <map>
#include <string>

#include "Parsing/Ast.hpp"
#include "SemanticAnalysis/SymbolTable.hpp"

namespace Parsing {

class UseResolver {
 public:
  struct Use {
    const ASTNode* node;
    std::string used_ident_name;

    friend bool operator<(const Use& left, const Use& right) {
      if (left.node != right.node) {
        return std::less<const ASTNode*>{}(left.node, right.node);
      }

      return left.used_ident_name < right.used_ident_name;
    }
  };

  const ASTNode* GetUsedVarDef(const std::string& name, const ASTNode* curr_node) const;

 private:
  UseResolver() = default;

  std::map<Use, const ASTNode*> use_to_definition_;

  friend UseResolver BuildUseResolver(
      const Program& program,
      SymbolTable& symbol_table);
};

UseResolver BuildUseResolver(const Program& program, SymbolTable& symbol_table);

}  // namespace Parsing
