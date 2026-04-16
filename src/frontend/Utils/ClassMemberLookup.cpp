#include "Utils/ClassMemberLookup.hpp"

#include <cassert>
#include <set>

namespace Front {

ClassMemberLookupResult ClassMemberLookupResult::CreateFieldResult(
    const ClassDeclarationStatement& declaring_class,
    const DeclarationStatement& field_declaration) {
  return ClassMemberLookupResult{
      .kind = ClassMemberKind::Field,
      .declaring_class = &declaring_class,
      .field_declaration = &field_declaration,
      .method_declaration = nullptr,
      .type = field_declaration.type};
}

ClassMemberLookupResult ClassMemberLookupResult::CreateMethodResult(
    const ClassDeclarationStatement& declaring_class,
    const FunctionDeclarationStatement& method_declaration) {
  return ClassMemberLookupResult{
      .kind = ClassMemberKind::Method,
      .declaring_class = &declaring_class,
      .field_declaration = nullptr,
      .method_declaration = &method_declaration,
      .type = method_declaration.function_type};
}

std::optional<ClassMemberLookupResult> LookupClassMember(
    const ClassType& start_class,
    const std::string& member_name,
    ClassMemberSearchMode search_mode) {
  std::set<const ClassType*> visited_classes{&start_class};
  const ClassType* current_class = &start_class;

  while (current_class != nullptr) {
    const ClassDeclarationStatement* current_class_decl = current_class->class_decl;
    if (current_class_decl == nullptr) {
      return std::nullopt;
    }

    for (const DeclarationStatement& field : current_class_decl->fields) {
      if (field.variable_name.name == member_name) {
        return ClassMemberLookupResult::CreateFieldResult(*current_class_decl, field);
      }
    }

    for (const FunctionDeclarationStatement& method : current_class_decl->methods) {
      if (method.function_name.name == member_name) {
        return ClassMemberLookupResult::CreateMethodResult(*current_class_decl, method);
      }
    }

    if (search_mode == ClassMemberSearchMode::CurrentClassOnly) {
      return std::nullopt;
    }

    const ClassType* base_class = current_class->base_class;
    if (base_class == nullptr) {
      return std::nullopt;
    }

    if (!visited_classes.insert(base_class).second) {
      assert(false && "cyclic inheritance");
      return std::nullopt;
    }

    current_class = base_class;
  }

  return std::nullopt;
}

}  // namespace Front
