#pragma once

#include <string>
#include <vector>

#include "Debug/Debug.hpp"
#include "Tokenizing/Tokens.hpp"

namespace Front {
class FrontendErrors;
}

namespace Tokenizing {

std::vector<TokenVariant> Tokenize(
    const std::string& input,
    const std::string& filename = "<input>",
    Front::FrontendErrors* errors = nullptr,
    std::vector<Front::DebugInfo>* token_debug_infos = nullptr);

}  // namespace Tokenizing
