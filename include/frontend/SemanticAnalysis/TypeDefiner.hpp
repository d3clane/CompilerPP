#pragma once

#include <map>
#include <string>

#include "Parsing/Ast.hpp"

namespace Parsing {

class TypeDefiner {
 public:
  const ClassDeclarationStatement* GetClassDeclaration(
      const std::string& class_name) const;

  const std::map<std::string, const ClassDeclarationStatement*>&
  GetClassDeclarations() const;

 private:
  std::map<std::string, const ClassDeclarationStatement*> class_declarations_;

  friend TypeDefiner BuildTypeDefiner(const Program& program);
};

TypeDefiner BuildTypeDefiner(const Program& program);

}  // namespace Parsing

