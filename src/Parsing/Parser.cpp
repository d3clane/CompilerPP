#include "Parsing/Parser.hpp"

#include <stdexcept>

#include "BisonParser.hpp"
#include "Tokenizing/Lexer.hpp"

namespace Parsing {

Program ParseTokens(const std::vector<Tokenizing::TokenVariant>& tokens) {
  Program parsed_program;
  ParserState parser_state{tokens, 0};
  BisonParser parser(parser_state, parsed_program);
  const int parse_status = parser.parse();
  if (parse_status != 0) {
    throw std::runtime_error("Parsing failed");
  }

  return parsed_program;
}

Program ParseSource(const std::string& source) {
  return ParseTokens(Tokenizing::Tokenize(source));
}

}  // namespace Parsing
