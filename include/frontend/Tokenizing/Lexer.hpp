#pragma once

#include <string>
#include <vector>

#include "Debug/Debug.hpp"
#include "Tokenizing/Tokens.hpp"

namespace Parsing {
class FrontendErrors;
}

namespace Tokenizing {

std::vector<TokenVariant> Tokenize(
    const std::string& input,
    const std::string& filename = "<input>",
    Parsing::FrontendErrors* errors = nullptr,
    std::vector<Parsing::DebugInfo>* token_debug_infos = nullptr);

}  // namespace Tokenizing
