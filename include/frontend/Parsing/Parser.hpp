#pragma once

#include <cstddef>
#include <vector>

#include "Debug/Debug.hpp"
#include "Debug/DebugCtx.hpp"
#include "Parsing/Ast.hpp"
#include "Tokenizing/Tokens.hpp"

namespace Parsing {

Program ParseTokens(
    const std::vector<Tokenizing::TokenVariant>& tokens,
    DebugCtx& debug_ctx,
    const std::vector<DebugInfo>* token_debug_infos = nullptr,
    size_t input_size = 0);

}  // namespace Parsing
