#pragma once

#include <map>
#include <vector>

#include "Parsing/Ast.hpp"

namespace Parsing {

class DebugCtx;
class FunctionRelationsVisitor;

class FunctionRelations {
 public:
  const FunctionDeclarationStatement* GetOwnerFunction(const ASTNode* node) const;
  const ClassDeclarationStatement* GetOwnerClass(
      const FunctionDeclarationStatement& function_declaration) const;
  const std::vector<const FunctionDeclarationStatement*>& GetFunctionsInEncounterOrder() const;

 private:
  std::vector<const FunctionDeclarationStatement*> functions_in_encounter_order_;
  std::map<const ASTNode*, const FunctionDeclarationStatement*> owner_function_by_node_;
  std::map<const FunctionDeclarationStatement*, const ClassDeclarationStatement*>
      owner_class_by_function_;

  friend FunctionRelations BuildFunctionRelations(
      const Program& program,
      DebugCtx& debug_ctx);
  friend class FunctionRelationsVisitor;
};

FunctionRelations BuildFunctionRelations(
    const Program& program,
    DebugCtx& debug_ctx);
FunctionRelations BuildFunctionRelations(const Program& program);

}  // namespace Parsing
