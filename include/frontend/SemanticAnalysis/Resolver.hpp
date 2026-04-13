#pragma once

#include <functional>
#include <map>
#include <string>

#include "Parsing/Ast.hpp"
#include "SemanticAnalysis/SymbolTable.hpp"

namespace Front {

class DebugCtx;

class UseResolver {
 public:
  struct ResolvedSymbol {
    const ASTNode* definition_node = nullptr;
    SymbolData symbol_data;
    const ClassDeclarationStatement* declaring_class = nullptr;
  };

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

  const ResolvedSymbol* GetResolvedSymbol(
      const std::string& name,
      const ASTNode* curr_node) const;
  const ASTNode* GetUsedVarDef(const std::string& name, const ASTNode* curr_node) const;

 private:
  UseResolver() = default;

  std::map<Use, ResolvedSymbol> use_to_resolved_symbol_;

  friend UseResolver BuildUseResolver(
      const Program& program,
      SymbolTable& symbol_table,
      DebugCtx& debug_ctx);
};

UseResolver BuildUseResolver(
    const Program& program,
    SymbolTable& symbol_table,
    DebugCtx& debug_ctx);
UseResolver BuildUseResolver(const Program& program, SymbolTable& symbol_table);

}  // namespace Front
