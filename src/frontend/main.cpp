#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "Debug/DebugCtx.hpp"
#include "Parsing/Parser.hpp"
#include "SemanticAnalysis/Resolver.hpp"
#include "SemanticAnalysis/SymbolTable.hpp"
#include "SemanticAnalysis/TypeChecker.hpp"
#include "Tokenizing/Lexer.hpp"
#include "Visitors/Interpreter.hpp"

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <input-file>\n";
    return 1;
  }

  const std::string input_path = argv[1];
  std::ifstream input_stream(input_path);
  if (!input_stream) {
    std::cerr << "Failed to open input file: " << input_path << "\n";
    return 1;
  }

  const std::string source{
      std::istreambuf_iterator<char>(input_stream),
      std::istreambuf_iterator<char>()};

  try {
    Parsing::DebugCtx debug_ctx(input_path);
    debug_ctx.SetInputCode(source);
    debug_ctx.GetErrors().SetLimit(20);

    std::vector<Parsing::DebugInfo> token_debug_infos;
    const std::vector<Tokenizing::TokenVariant> tokens =
        Tokenizing::Tokenize(
            source,
            input_path,
            &debug_ctx.GetErrors(),
            &token_debug_infos);

    const Parsing::Program program = Parsing::ParseTokens(
        tokens,
        debug_ctx,
        &token_debug_infos,
        source.size());

    Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program, debug_ctx);
    const Parsing::UseResolver use_resolver =
        Parsing::BuildUseResolver(program, symbol_table, debug_ctx);
    Parsing::CheckTypes(program, use_resolver, symbol_table, debug_ctx);

    if (debug_ctx.GetErrors().HasErrors()) {
      debug_ctx.GetErrors().ThrowErrors();
    }

    Parsing::Interpret(program, std::cout);
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << "\n";
    return 1;
  }

  return 0;
}
