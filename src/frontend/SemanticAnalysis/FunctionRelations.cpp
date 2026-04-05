#include "SemanticAnalysis/FunctionRelations.hpp"

#include <cassert>
#include <variant>

#include "Debug/DebugCtx.hpp"
#include "Utils/Overload.hpp"

namespace Parsing {

class FunctionRelationsVisitor {
 public:
  explicit FunctionRelationsVisitor(FunctionRelations& relations)
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

  void VisitStatements(const List<Statement>& statements) {
    for (size_t i = 0; i < statements.size(); ++i) {
      assert(statements[i] != nullptr);
      VisitStatement(*statements[i]);
    }
  }

  void VisitTopLevelStatement(const Statement& statement) {
    std::visit(
        Utils::Overload{
            [this](const FunctionDeclarationStatement& function_declaration) {
              VisitFunctionDeclaration(function_declaration);
            },
            [this](const ClassDeclarationStatement& class_declaration) {
              VisitClassDeclaration(class_declaration);
            },
            [this](const auto&) {
            }},
        statement.value);
  }

  void VisitStatement(const Statement& statement) {
    std::visit(
        Utils::Overload{
            [this](const DeclarationStatement& declaration) {
              if (current_function_ != nullptr) {
                relations_.owner_function_by_node_[&declaration] = current_function_;
              }
            },
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
    relations_.functions_in_encounter_order_.push_back(&function_declaration);
    if (current_class_ != nullptr) {
      relations_.owner_class_by_function_[&function_declaration] = current_class_;
    }

    const FunctionDeclarationStatement* previous_function = current_function_;
    const ClassDeclarationStatement* previous_class = current_class_;
    current_function_ = &function_declaration;
    current_class_ = nullptr;

    for (size_t i = 0; i < function_declaration.parameters.size(); ++i) {
      relations_.owner_function_by_node_[&function_declaration.parameters[i]] =
          &function_declaration;
    }

    assert(function_declaration.body != nullptr);
    VisitStatements(function_declaration.body->statements);

    current_function_ = previous_function;
    current_class_ = previous_class;
  }

  void VisitClassDeclaration(const ClassDeclarationStatement& class_declaration) {
    const ClassDeclarationStatement* previous_class = current_class_;
    current_class_ = &class_declaration;
    for (size_t i = 0; i < class_declaration.methods.size(); ++i) {
      VisitFunctionDeclaration(class_declaration.methods[i]);
    }
    current_class_ = previous_class;
  }

  FunctionRelations& relations_;
  const FunctionDeclarationStatement* current_function_ = nullptr;
  const ClassDeclarationStatement* current_class_ = nullptr;
};

const FunctionDeclarationStatement* FunctionRelations::GetOwnerFunction(
    const ASTNode* node) const {
  const auto owner_it = owner_function_by_node_.find(node);
  if (owner_it == owner_function_by_node_.end()) {
    return nullptr;
  }

  return owner_it->second;
}

const ClassDeclarationStatement* FunctionRelations::GetOwnerClass(
    const FunctionDeclarationStatement& function_declaration) const {
  const auto owner_it = owner_class_by_function_.find(&function_declaration);
  if (owner_it == owner_class_by_function_.end()) {
    return nullptr;
  }

  return owner_it->second;
}

const std::vector<const FunctionDeclarationStatement*>&
FunctionRelations::GetFunctionsInEncounterOrder() const {
  return functions_in_encounter_order_;
}

FunctionRelations BuildFunctionRelations(
    const Program& program,
    DebugCtx&) {
  FunctionRelations relations;
  FunctionRelationsVisitor visitor(relations);
  visitor.Build(program);
  return relations;
}

FunctionRelations BuildFunctionRelations(const Program& program) {
  DebugCtx debug_ctx;
  return BuildFunctionRelations(program, debug_ctx);
}

}  // namespace Parsing
