#include "Parsing/Parser.hpp"

#include <stdexcept>
#include <vector>

#include "BisonParser.hpp"
#include "Debug/Debug.hpp"
#include "Debug/Errors.hpp"
#include "Tokenizing/Lexer.hpp"

namespace Parsing {

namespace {

Program ParseTokensImpl(
    const std::vector<Tokenizing::TokenVariant>& tokens,
    const std::vector<DebugInfo>* token_debug_infos,
    FrontendErrors* errors,
    const std::string& filename,
    size_t input_size) {
  Program parsed_program;
  ParserState parser_state{
      tokens,
      0,
      token_debug_infos,
      errors,
      filename,
      input_size,
      std::vector<PendingErrorState>()};

  BisonParser parser(parser_state, parsed_program);
  const int parse_status = parser.parse();

  if (parse_status != 0) {
    if (errors != nullptr && errors->HasErrors()) {
      errors->ThrowErrors();
    }
    throw std::runtime_error("Parsing failed");
  }

  if (errors != nullptr && errors->HasErrors()) {
    errors->ThrowErrors();
  }

  return parsed_program;
}

}  // namespace

Program ParseTokens(const std::vector<Tokenizing::TokenVariant>& tokens) {
  return ParseTokensImpl(tokens, nullptr, nullptr, "<tokens>", 0);
}

Program ParseSource(const std::string& source, const std::string& filename) {
  ASTDebugInfo ast_debug_info;
  ast_debug_info.SetInputCode(source);
  FrontendErrors frontend_errors(ast_debug_info);
  frontend_errors.SetLimit(50);
  std::vector<DebugInfo> token_debug_infos;
  const std::vector<Tokenizing::TokenVariant> tokens =
      Tokenizing::Tokenize(
          source,
          filename,
          &frontend_errors,
          &token_debug_infos);
  return ParseTokensImpl(
      tokens,
      &token_debug_infos,
      &frontend_errors,
      filename,
      source.size());
}

}  // namespace Parsing
