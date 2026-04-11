#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Debug/DebugCtx.hpp"
#include "Lowering/LLVMIRLowering.hpp"
#include "Lowering/ObjectFileLowering.hpp"
#include "Parsing/Parser.hpp"
#include "SemanticAnalysis/AccessAllowanceChecker.hpp"
#include "SemanticAnalysis/Resolver.hpp"
#include "SemanticAnalysis/StatementNumerizer.hpp"
#include "SemanticAnalysis/SymbolTable.hpp"
#include "SemanticAnalysis/TypeChecker.hpp"
#include "SemanticAnalysis/TypeDefiner.hpp"
#include "Tokenizing/Lexer.hpp"

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: " << argv[0] << " <input-file> [output-object-file]\n";
    return 1;
  }

  const std::string input_path = argv[1];
  std::filesystem::path output_path;
  if (argc == 3) {
    output_path = argv[2];
  } else {
    output_path = input_path;
    output_path.replace_extension(".o");
  }

  std::ifstream input_stream(input_path);
  if (!input_stream) {
    std::cerr << "Failed to open input file: " << input_path << "\n";
    return 1;
  }

  const std::string source{
      std::istreambuf_iterator<char>(input_stream),
      std::istreambuf_iterator<char>()};

  std::optional<Parsing::DebugCtx> debug_ctx;
  try {
    debug_ctx.emplace(input_path);
    debug_ctx->SetInputCode(source);
    debug_ctx->GetErrors().SetLimit(20);

    std::vector<Parsing::DebugInfo> token_debug_infos;
    const std::vector<Tokenizing::TokenVariant> tokens =
        Tokenizing::Tokenize(
            source,
            input_path,
            &debug_ctx->GetErrors(),
            &token_debug_infos);

    const Parsing::Program program = Parsing::ParseTokens(
        tokens,
        *debug_ctx,
        &token_debug_infos,
        source.size());

    Parsing::StatementNumerizer numerizer =
        Parsing::BuildStatementNumerizer(program);
    const Parsing::TypeDefiner type_definer =
        Parsing::BuildTypeDefiner(program);
    Parsing::SymbolTable symbol_table =
        Parsing::BuildSymbolTable(
            program,
            std::move(numerizer),
            *debug_ctx);
    const Parsing::UseResolver use_resolver =
        Parsing::BuildUseResolver(program, symbol_table, *debug_ctx);
    Parsing::CheckAccessAllowance(
        program,
        use_resolver,
        type_definer,
        *debug_ctx);
    Parsing::CheckTypes(
        program,
        use_resolver,
        type_definer,
        *debug_ctx);

    if (debug_ctx->GetErrors().HasErrors()) {
      debug_ctx->GetErrors().ThrowErrors();
    }

    Parsing::LLVMIRModule llvm_ir = Parsing::LowerToLLVMIRModule(
        program,
        use_resolver,
        type_definer);
    Parsing::LowerToObjectFile(llvm_ir.GetModule(), output_path.string());
  } catch (const std::exception& error) {
    if (debug_ctx.has_value() && debug_ctx->GetErrors().HasErrors()) {
      try {
        debug_ctx->GetErrors().ThrowErrors();
      } catch (const std::exception& debug_error) {
        if (std::string(debug_error.what()) != error.what()) {
          std::cerr << "Error: " << debug_error.what() << "\n";
        }
      }
    }

    std::cerr << "Error: " << error.what() << "\n";
    return 1;
  }

  return 0;
}
