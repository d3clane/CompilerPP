#include "Utils/ClassMemberLookup.hpp"

#include <cassert>
#include <set>

namespace Parsing {

std::optional<ClassMemberLookupResult> LookupClassMember(
    const ClassType& start_class,
    const std::string& member_name,
    ClassMemberSearchMode search_mode) {
  std::set<const ClassType*> visited_classes{&start_class};
  const ClassType* current_class = &start_class;

  while (current_class != nullptr) {
    const ClassDeclarationStatement* current_class_decl = current_class->parent;
    if (current_class_decl == nullptr) {
      return std::nullopt;
    }

    for (const DeclarationStatement& field : current_class_decl->fields) {
      if (field.variable_name.name == member_name) {
        return ClassMemberLookupResult{
            .kind = ClassMemberKind::Field,
            .declaring_class = current_class_decl,
            .field_declaration = &field,
            .type = field.type};
      }
    }

    for (const FunctionDeclarationStatement& method : current_class_decl->methods) {
      if (method.function_name.name == member_name) {
        return ClassMemberLookupResult{
            .kind = ClassMemberKind::Method,
            .declaring_class = current_class_decl,
            .method_declaration = &method,
            .type = method.function_type};
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

}  // namespace Parsing
