#pragma once

#include <optional>
#include <string>

#include "Parsing/Ast.hpp"

namespace Parsing {

enum class ClassMemberSearchMode {
  CurrentClassOnly,
  CurrentClassAndBases,
};

enum class ClassMemberKind {
  Field,
  Method,
};

struct ClassMemberLookupResult {
  ClassMemberKind kind = ClassMemberKind::Field;
  const ClassDeclarationStatement* declaring_class = nullptr;
  const DeclarationStatement* field_declaration = nullptr;
  const FunctionDeclarationStatement* method_declaration = nullptr;
  const Type* type = nullptr;

  static ClassMemberLookupResult CreateFieldResult(
      const ClassDeclarationStatement& declaring_class,
      const DeclarationStatement& field_declaration);
  static ClassMemberLookupResult CreateMethodResult(
      const ClassDeclarationStatement& declaring_class,
      const FunctionDeclarationStatement& method_declaration);
};

std::optional<ClassMemberLookupResult> LookupClassMember(
    const ClassType& start_class,
    const std::string& member_name,
    ClassMemberSearchMode search_mode);

}  // namespace Parsing
