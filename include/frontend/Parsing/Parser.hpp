#pragma once

#include <string>
#include <vector>

#include "Parsing/Ast.hpp"
#include "Tokenizing/Tokens.hpp"

namespace Parsing {

Program ParseTokens(const std::vector<Tokenizing::TokenVariant>& tokens);
Program ParseSource(const std::string& source, const std::string& filename = "<input>");

}  // namespace Parsing
