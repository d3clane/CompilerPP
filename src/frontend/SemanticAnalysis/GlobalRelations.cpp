#include "SemanticAnalysis/GlobalRelations.hpp"

#include <cassert>
#include <variant>

#include "Debug/DebugCtx.hpp"
#include "Utils/Overload.hpp"

namespace Parsing {

class GlobalRelationsVisitor {
 public:
  explicit GlobalRelationsVisitor(GlobalRelations& relations)
      : relations_(relations) {}

  void Build(const Program& program) {
    VisitTopLevelStatements(program.top_statements);
  }

 private:
  void VisitTopLevelStatements(const List<Statement>& statements) {
    for (size_t i = 0; i < statements.size(); ++i) {
      assert(statements[i] != nullptr);
      VisitTopLevelStatement(*statements[i]);
    }
  }

  void VisitTopLevelStatement(const Statement& statement) {
    std::visit(
        Utils::Overload{
            [this](const DeclarationStatement& declaration) {
              relations_.global_declarations_in_encounter_order_.push_back(&declaration);
            },
            [this](const FunctionDeclarationStatement& function_declaration) {
              if (function_declaration.function_name == "main" &&
                  relations_.user_main_ == nullptr) {
                relations_.user_main_ = &function_declaration;
              }
            },
            [](const auto&) {
            }},
        statement.value);
  }

  GlobalRelations& relations_;
};

const std::vector<const DeclarationStatement*>&
GlobalRelations::GetGlobalDeclarationsInEncounterOrder() const {
  return global_declarations_in_encounter_order_;
}

const FunctionDeclarationStatement* GlobalRelations::GetUserMain() const {
  return user_main_;
}

GlobalRelations BuildGlobalRelations(
    const Program& program,
    DebugCtx&) {
  GlobalRelations relations;
  GlobalRelationsVisitor visitor(relations);
  visitor.Build(program);
  return relations;
}

GlobalRelations BuildGlobalRelations(const Program& program) {
  DebugCtx debug_ctx;
  return BuildGlobalRelations(program, debug_ctx);
}

}  // namespace Parsing
