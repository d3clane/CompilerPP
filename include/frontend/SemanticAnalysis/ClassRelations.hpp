#pragma once

#include <map>
#include <optional>
#include <vector>

#include "Parsing/Ast.hpp"
#include "SemanticAnalysis/TypeDefiner.hpp"

namespace Parsing {

class DebugCtx;
class ClassRelationsVisitor;
class ClassRelationsBuilder;

class ClassRelations {
 public:
  const ClassDeclarationStatement* GetFieldOwner(const ASTNode* field_node) const;
  const ClassDeclarationStatement* GetMethodOwner(
      const FunctionDeclarationStatement& function_declaration) const;
  const ClassDeclarationStatement* GetBaseClass(
      const ClassDeclarationStatement& class_declaration) const;
  std::optional<size_t> GetLocalFieldIndex(const ASTNode* field_node) const;
  const std::vector<const ClassDeclarationStatement*>& GetClassesInEncounterOrder() const;
  const std::vector<const ClassDeclarationStatement*>& GetClassesBaseFirstOrder() const;
  const std::vector<const ClassDeclarationStatement*>& GetInheritanceChainFromRoot(
      const ClassDeclarationStatement& class_declaration) const;

 private:
  std::vector<const ClassDeclarationStatement*> classes_in_encounter_order_;
  std::vector<const ClassDeclarationStatement*> classes_base_first_order_;
  std::map<const ASTNode*, const ClassDeclarationStatement*> field_owner_by_node_;
  std::map<const FunctionDeclarationStatement*, const ClassDeclarationStatement*>
      method_owner_by_function_;
  std::map<const ASTNode*, size_t> local_field_index_by_node_;
  std::map<const ClassDeclarationStatement*, const ClassDeclarationStatement*>
      base_class_by_class_;
  std::map<const ClassDeclarationStatement*, std::vector<const ClassDeclarationStatement*>>
      inheritance_chain_from_root_by_class_;

  friend ClassRelations BuildClassRelations(
      const Program& program,
      const TypeDefiner& type_definer,
      DebugCtx& debug_ctx);
  friend class ClassRelationsVisitor;
  friend class ClassRelationsBuilder;
};

ClassRelations BuildClassRelations(
    const Program& program,
    const TypeDefiner& type_definer,
    DebugCtx& debug_ctx);
ClassRelations BuildClassRelations(
    const Program& program,
    const TypeDefiner& type_definer);

}  // namespace Parsing
