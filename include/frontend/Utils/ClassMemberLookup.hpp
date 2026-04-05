#pragma once

#include <optional>
#include <string>

#include "Parsing/Ast.hpp"
#include "SemanticAnalysis/SymbolTable.hpp"
#include "SemanticAnalysis/TypeDefiner.hpp"

namespace Parsing {

enum class ClassMemberSearchMode {
  CurrentClassOnly,
  CurrentClassAndBases,
};

struct ClassMemberLookupResult {
  const ClassDeclarationStatement* declaring_class = nullptr;
  const SymbolData* symbol_data = nullptr;
};

std::optional<ClassMemberLookupResult> LookupClassMember(
    const ClassDeclarationStatement& start_class,
    const std::string& member_name,
    ClassMemberSearchMode search_mode,
    const SymbolTable& symbol_table,
    const TypeDefiner& type_definer);

}  // namespace Parsing
