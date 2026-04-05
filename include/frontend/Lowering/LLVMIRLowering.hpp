#pragma once

#include <string>

#include "Parsing/Ast.hpp"

namespace Parsing {

class TypeDefiner;
class UseResolver;

std::string LowerToLLVMIR(
    const Program& program,
    const UseResolver& use_resolver,
    const TypeDefiner& type_definer);

}  // namespace Parsing
