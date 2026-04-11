#pragma once

#include <vector>

#include "Parsing/Ast.hpp"

namespace Parsing {

class DebugCtx;
class GlobalRelationsVisitor;

class GlobalRelations {
 public:
  const std::vector<const DeclarationStatement*>& GetGlobalDeclarationsInEncounterOrder() const;
  const FunctionDeclarationStatement* GetUserMain() const;

 private:
  std::vector<const DeclarationStatement*> global_declarations_in_encounter_order_;
  const FunctionDeclarationStatement* user_main_ = nullptr;

  friend GlobalRelations BuildGlobalRelations(
      const Program& program,
      DebugCtx& debug_ctx);
  friend class GlobalRelationsVisitor;
};

GlobalRelations BuildGlobalRelations(
    const Program& program,
    DebugCtx& debug_ctx);
GlobalRelations BuildGlobalRelations(const Program& program);

}  // namespace Parsing
