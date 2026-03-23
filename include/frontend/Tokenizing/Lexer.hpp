#pragma once

#include <string>
#include <vector>

#include "Tokenizing/Tokens.hpp"

namespace Tokenizing {

std::vector<TokenVariant> Tokenize(const std::string& input);

}  // namespace Tokenizing
