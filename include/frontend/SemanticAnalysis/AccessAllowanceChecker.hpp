#pragma once

#include "Parsing/Ast.hpp"
#include "SemanticAnalysis/Resolver.hpp"
#include "SemanticAnalysis/TypeDefiner.hpp"

namespace Parsing {

class DebugCtx;

void CheckAccessAllowance(
    const Program& program,
    const UseResolver& use_resolver,
    const TypeDefiner& type_definer,
    DebugCtx& debug_ctx);

void CheckAccessAllowance(
    const Program& program,
    const UseResolver& use_resolver,
    const TypeDefiner& type_definer);

}  // namespace Parsing

