#pragma once

#include <string>
#include <vector>

#include "Debug/DebugCtx.hpp"
#include "Parsing/Parser.hpp"
#include "Tokenizing/Lexer.hpp"

namespace Parsing {

inline Program ParseSource(
    const std::string& source,
    const std::string& filename = "<input>") {
  DebugCtx debug_ctx(filename);
  debug_ctx.SetInputCode(source);
  debug_ctx.GetErrors().SetLimit(1000);

  std::vector<DebugInfo> token_debug_infos;
  const std::vector<Tokenizing::TokenVariant> tokens =
      Tokenizing::Tokenize(
          source,
          filename,
          &debug_ctx.GetErrors(),
          &token_debug_infos);

  Program program =
      ParseTokens(tokens, debug_ctx, &token_debug_infos, source.size());
  if (debug_ctx.GetErrors().HasErrors()) {
    debug_ctx.GetErrors().ThrowErrors();
  }

  return program;
}

}  // namespace Parsing
