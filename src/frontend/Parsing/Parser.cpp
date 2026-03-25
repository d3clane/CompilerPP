#include "Parsing/Parser.hpp"

#include <vector>

#include "BisonParser.hpp"
#include "Debug/Debug.hpp"
#include "Debug/DebugCtx.hpp"

namespace Parsing {

namespace {

Program ParseTokensImpl(
    const std::vector<Tokenizing::TokenVariant>& tokens,
    DebugCtx& debug_ctx,
    const std::vector<DebugInfo>* token_debug_infos,
    size_t input_size) {
  FrontendErrors& errors = debug_ctx.GetErrors();
  Program parsed_program;
  ParserState parser_state{
      tokens,
      0,
      token_debug_infos,
      &errors,
      debug_ctx.GetFilename(),
      input_size,
      std::vector<PendingErrorState>()};

  BisonParser parser(parser_state, parsed_program);
  const int parse_status = parser.parse();

  if (parse_status != 0) {
    errors.AddError(
        CreateDebugInfo(
            debug_ctx.GetFilename(),
            {0, 0},
            {0, 0},
            {0, input_size}),
        "Parsing failed");
  }

  return parsed_program;
}

}  // namespace

Program ParseTokens(
    const std::vector<Tokenizing::TokenVariant>& tokens,
    DebugCtx& debug_ctx,
    const std::vector<DebugInfo>* token_debug_infos,
    size_t input_size) {
  return ParseTokensImpl(tokens, debug_ctx, token_debug_infos, input_size);
}

}  // namespace Parsing
