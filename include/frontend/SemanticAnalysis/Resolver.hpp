#pragma once

#include <map>
#include <string>

#include "Parsing/Ast.hpp"
#include "SemanticAnalysis/SymbolTable.hpp"

namespace Parsing {

class UseResolver {
 public:
  AstNodeID GetUsedVarDef(const std::string& name, ASTNode* curr_node) const;
  AstNodeID GetUsedVarDef(const std::string& name, AstNodeID curr_node_id) const;

 private:
  UseResolver() = default;

  struct Use {
    AstNodeID node_id;
    std::string used_ident_name;

    friend bool operator<(const Use& left, const Use& right) {
      if (left.node_id != right.node_id) {
        return left.node_id < right.node_id;
      }

      return left.used_ident_name < right.used_ident_name;
    }
  };

  std::map<Use, AstNodeID> use_to_definition_;

  friend UseResolver BuildUseResolver(
      const Program& program,
      SymbolTable& symbol_table);
};

UseResolver BuildUseResolver(const Program& program, SymbolTable& symbol_table);

}  // namespace Parsing
