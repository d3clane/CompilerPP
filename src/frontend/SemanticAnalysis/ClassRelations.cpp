#include "SemanticAnalysis/ClassRelations.hpp"

#include <cassert>
#include <set>
#include <stdexcept>
#include <string>
#include <variant>

#include "Debug/DebugCtx.hpp"
#include "Utils/Overload.hpp"

namespace Parsing {

class ClassRelationsVisitor {
 public:
  explicit ClassRelationsVisitor(ClassRelations& relations)
      : relations_(relations) {}

  void Build(const Program& program) {
    VisitStatements(program.top_statements);
  }

 private:
  void VisitStatements(const List<Statement>& statements) {
    for (size_t i = 0; i < statements.size(); ++i) {
      assert(statements[i] != nullptr);
      VisitStatement(*statements[i]);
    }
  }

  void VisitStatement(const Statement& statement) {
    std::visit(
        Utils::Overload{
            [this](const FunctionDeclarationStatement& function_declaration) {
              VisitFunctionDeclaration(function_declaration);
            },
            [this](const ClassDeclarationStatement& class_declaration) {
              VisitClassDeclaration(class_declaration);
            },
            [this](const IfStatement& if_statement) {
              if (if_statement.true_block != nullptr) {
                VisitStatements(if_statement.true_block->statements);
              }

              if (if_statement.else_tail != nullptr) {
                VisitElseTail(*if_statement.else_tail);
              }
            },
            [this](const Block& block) {
              VisitStatements(block.statements);
            },
            [](const auto&) {
            }},
        statement.value);
  }

  void VisitElseTail(const ElseTail& else_tail) {
    if (else_tail.else_if != nullptr) {
      if (else_tail.else_if->true_block != nullptr) {
        VisitStatements(else_tail.else_if->true_block->statements);
      }

      if (else_tail.else_if->else_tail != nullptr) {
        VisitElseTail(*else_tail.else_if->else_tail);
      }
      return;
    }

    if (else_tail.else_block != nullptr) {
      VisitStatements(else_tail.else_block->statements);
    }
  }

  void VisitFunctionDeclaration(const FunctionDeclarationStatement& function_declaration) {
    const ClassDeclarationStatement* previous_class = current_class_;
    current_class_ = nullptr;

    assert(function_declaration.body != nullptr);
    VisitStatements(function_declaration.body->statements);

    current_class_ = previous_class;
  }

  void VisitClassDeclaration(const ClassDeclarationStatement& class_declaration) {
    relations_.classes_in_encounter_order_.push_back(&class_declaration);

    const ClassType* class_type = AsClassType(class_declaration.class_type);
    if (class_type != nullptr &&
        class_type->base_class != nullptr) {
      unresolved_base_type_by_class_[&class_declaration] =
          class_type->base_class;
    }

    for (size_t i = 0; i < class_declaration.fields.size(); ++i) {
      relations_.field_owner_by_node_[&class_declaration.fields[i]] = &class_declaration;
      relations_.local_field_index_by_node_[&class_declaration.fields[i]] = i;
    }

    const ClassDeclarationStatement* previous_class = current_class_;
    current_class_ = &class_declaration;
    for (size_t i = 0; i < class_declaration.methods.size(); ++i) {
      relations_.method_owner_by_function_[&class_declaration.methods[i]] = &class_declaration;
      VisitFunctionDeclaration(class_declaration.methods[i]);
    }
    current_class_ = previous_class;
  }

  ClassRelations& relations_;
  const ClassDeclarationStatement* current_class_ = nullptr;
  std::map<const ClassDeclarationStatement*, const ClassType*> unresolved_base_type_by_class_;

  friend class ClassRelationsBuilder;
};

class ClassRelationsBuilder {
 public:
  ClassRelationsBuilder(
      const Program& program,
      DebugCtx& debug_ctx)
      : program_(program),
        debug_ctx_(debug_ctx) {}

  ClassRelations Build() {
    ClassRelations relations;
    ClassRelationsVisitor visitor(relations);
    visitor.Build(program_);

    for (const auto& [class_declaration, base_class_type] :
         visitor.unresolved_base_type_by_class_) {
      const ClassDeclarationStatement* base_class = base_class_type->parent;
      if (base_class != nullptr) {
        relations.base_class_by_class_[class_declaration] = base_class;
      }
    }

    std::set<const ClassDeclarationStatement*> permanently_visited;
    std::set<const ClassDeclarationStatement*> stack_visited;
    for (size_t i = 0; i < relations.classes_in_encounter_order_.size(); ++i) {
      BuildClassOrderAndChains(
          *relations.classes_in_encounter_order_[i],
          relations,
          permanently_visited,
          stack_visited);
    }

    return relations;
  }

 private:
  void ReportCodeError(const ASTNode* node, const std::string& message) const {
    debug_ctx_.GetErrors().AddError(node, message);
  }

  void BuildClassOrderAndChains(
      const ClassDeclarationStatement& class_declaration,
      ClassRelations& relations,
      std::set<const ClassDeclarationStatement*>& permanently_visited,
      std::set<const ClassDeclarationStatement*>& stack_visited) {
    if (permanently_visited.contains(&class_declaration)) {
      return;
    }

    if (!stack_visited.insert(&class_declaration).second) {
      ReportCodeError(
          &class_declaration,
          "cyclic inheritance involving class " + class_declaration.class_name.name);
      return;
    }

    std::vector<const ClassDeclarationStatement*> chain;
    const ClassDeclarationStatement* base_class =
        relations.GetBaseClass(class_declaration);
    if (base_class != nullptr) {
      BuildClassOrderAndChains(
          *base_class,
          relations,
          permanently_visited,
          stack_visited);
      const auto chain_it =
          relations.inheritance_chain_from_root_by_class_.find(base_class);
      if (chain_it != relations.inheritance_chain_from_root_by_class_.end()) {
        chain = chain_it->second;
      }
    }

    chain.push_back(&class_declaration);
    relations.inheritance_chain_from_root_by_class_[&class_declaration] =
        std::move(chain);

    stack_visited.erase(&class_declaration);
    permanently_visited.insert(&class_declaration);
    relations.classes_base_first_order_.push_back(&class_declaration);
  }

  const Program& program_;
  DebugCtx& debug_ctx_;
};

const ClassDeclarationStatement* ClassRelations::GetFieldOwner(
    const ASTNode* field_node) const {
  const auto field_it = field_owner_by_node_.find(field_node);
  if (field_it == field_owner_by_node_.end()) {
    return nullptr;
  }

  return field_it->second;
}

const ClassDeclarationStatement* ClassRelations::GetMethodOwner(
    const FunctionDeclarationStatement& function_declaration) const {
  const auto owner_it = method_owner_by_function_.find(&function_declaration);
  if (owner_it == method_owner_by_function_.end()) {
    return nullptr;
  }

  return owner_it->second;
}

const ClassDeclarationStatement* ClassRelations::GetBaseClass(
    const ClassDeclarationStatement& class_declaration) const {
  const auto base_it = base_class_by_class_.find(&class_declaration);
  if (base_it == base_class_by_class_.end()) {
    return nullptr;
  }

  return base_it->second;
}

std::optional<size_t> ClassRelations::GetLocalFieldIndex(const ASTNode* field_node) const {
  const auto field_it = local_field_index_by_node_.find(field_node);
  if (field_it == local_field_index_by_node_.end()) {
    return std::nullopt;
  }

  return field_it->second;
}

const std::vector<const ClassDeclarationStatement*>&
ClassRelations::GetClassesInEncounterOrder() const {
  return classes_in_encounter_order_;
}

const std::vector<const ClassDeclarationStatement*>&
ClassRelations::GetClassesBaseFirstOrder() const {
  return classes_base_first_order_;
}

const std::vector<const ClassDeclarationStatement*>&
ClassRelations::GetInheritanceChainFromRoot(
    const ClassDeclarationStatement& class_declaration) const {
  const auto chain_it = inheritance_chain_from_root_by_class_.find(&class_declaration);
  if (chain_it == inheritance_chain_from_root_by_class_.end()) {
    throw std::runtime_error("class relations are incorrect for this program");
  }

  return chain_it->second;
}

ClassRelations BuildClassRelations(
    const Program& program,
    DebugCtx& debug_ctx) {
  ClassRelationsBuilder builder(program, debug_ctx);
  return builder.Build();
}

ClassRelations BuildClassRelations(
    const Program& program) {
  DebugCtx debug_ctx;
  ClassRelations relations = BuildClassRelations(program, debug_ctx);
  if (debug_ctx.GetErrors().HasErrors()) {
    debug_ctx.GetErrors().ThrowErrors();
  }

  return relations;
}

}  // namespace Parsing
