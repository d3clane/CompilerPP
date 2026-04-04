#include "SemanticAnalysis/TypeDefiner.hpp"

#include <cassert>
#include <variant>

#include "Utils/Overload.hpp"

namespace Parsing {

namespace {

class TypeDefinerVisitor {
 public:
  std::map<std::string, const ClassDeclarationStatement*> Build(
      const Program& program) {
    VisitStatements(program.top_statements);
    return std::move(class_declarations_);
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
            [this](const ClassDeclarationStatement& class_declaration) {
              VisitClassDeclaration(class_declaration);
            },
            [this](const FunctionDeclarationStatement& function_declaration) {
              assert(function_declaration.body != nullptr);
              VisitStatements(function_declaration.body->statements);
            },
            [this](const IfStatement& if_statement) {
              assert(if_statement.true_block != nullptr);
              assert(if_statement.else_tail != nullptr);
              VisitStatements(if_statement.true_block->statements);
              VisitElseTail(*if_statement.else_tail);
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
      assert(else_tail.else_if != nullptr);
      assert(else_tail.else_if->true_block != nullptr);
      assert(else_tail.else_if->else_tail != nullptr);
      VisitStatements(else_tail.else_if->true_block->statements);
      VisitElseTail(*else_tail.else_if->else_tail);
      return;
    }

    if (else_tail.else_block != nullptr) {
      VisitStatements(else_tail.else_block->statements);
    }
  }

  void VisitClassDeclaration(const ClassDeclarationStatement& class_declaration) {
    class_declarations_[class_declaration.class_name] =
        &class_declaration;

    for (size_t i = 0; i < class_declaration.methods.size(); ++i) {
      assert(class_declaration.methods[i].body != nullptr);
      VisitStatements(class_declaration.methods[i].body->statements);
    }
  }

  std::map<std::string, const ClassDeclarationStatement*> class_declarations_;
};

}  // namespace

const ClassDeclarationStatement* TypeDefiner::GetClassDeclaration(
    const std::string& class_name) const {
  const auto class_it = class_declarations_.find(class_name);
  if (class_it == class_declarations_.end()) {
    return nullptr;
  }

  return class_it->second;
}

const std::map<std::string, const ClassDeclarationStatement*>&
TypeDefiner::GetClassDeclarations() const {
  return class_declarations_;
}

TypeDefiner BuildTypeDefiner(const Program& program) {
  TypeDefinerVisitor visitor;
  TypeDefiner type_definer;
  type_definer.class_declarations_ = visitor.Build(program);
  return type_definer;
}

}  // namespace Parsing
