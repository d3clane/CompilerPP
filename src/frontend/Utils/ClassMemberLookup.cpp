#include "Utils/ClassMemberLookup.hpp"

#include <cassert>
#include <set>
#include <stdexcept>

namespace Parsing {

std::optional<ClassMemberLookupResult> LookupClassMember(
    const ClassDeclarationStatement& start_class,
    const std::string& member_name,
    ClassMemberSearchMode search_mode,
    const SymbolTable& symbol_table,
    const TypeDefiner& type_definer) {
  std::set<std::string> visited_classes{start_class.class_name};
  const ClassDeclarationStatement* current_class = &start_class;

  while (current_class != nullptr) {
    const LocalSymbolTable* class_scope = symbol_table.GetTable(current_class);
    if (class_scope == nullptr) {
      throw std::runtime_error("symbol table is incorrect for this program");
    }

    const SymbolData* symbol_data =
        class_scope->GetSymbolInfoInLocalScope(member_name);
    if (symbol_data != nullptr) {
      return ClassMemberLookupResult{
          current_class,
          symbol_data};
    }

    if (search_mode == ClassMemberSearchMode::CurrentClassOnly ||
        !current_class->base_class_name.has_value()) {
      return std::nullopt;
    }

    const ClassDeclarationStatement* base_class =
        type_definer.GetClassDeclaration(*current_class->base_class_name);
    if (base_class == nullptr) {
      return std::nullopt;
    }

    if (!visited_classes.insert(base_class->class_name).second) {
      assert(false && "Cyclic inheritance");
      return std::nullopt;
    }

    current_class = base_class;
  }

  return std::nullopt;
}

}  // namespace Parsing
