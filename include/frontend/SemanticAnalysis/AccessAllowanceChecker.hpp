#pragma once

#include "Parsing/Ast.hpp"
#include "SemanticAnalysis/Resolver.hpp"

namespace Parsing {

class DebugCtx;

void CheckAccessAllowance(
    const Program& program,
    const UseResolver& use_resolver,
    DebugCtx& debug_ctx);

void CheckAccessAllowance(
    const Program& program,
    const UseResolver& use_resolver);

}  // namespace Parsing
