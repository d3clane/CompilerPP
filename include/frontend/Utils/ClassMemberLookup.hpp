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
};

std::optional<ClassMemberLookupResult> LookupClassMember(
    const ClassType& start_class,
    const std::string& member_name,
    ClassMemberSearchMode search_mode);

}  // namespace Parsing
