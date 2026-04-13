#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Debug/DebugCtx.hpp"
#include "Lowering/ExecutableLowering.hpp"
#include "Lowering/LLVMIRLowering.hpp"
#include "Lowering/ObjectFileLowering.hpp"
#include "Parsing/Parser.hpp"
#include "SemanticAnalysis/AccessAllowanceChecker.hpp"
#include "SemanticAnalysis/Resolver.hpp"
#include "SemanticAnalysis/StatementNumerizer.hpp"
#include "SemanticAnalysis/SymbolTable.hpp"
#include "SemanticAnalysis/TypeChecker.hpp"
#include "Tokenizing/Lexer.hpp"

int main(int argc, char* argv[]) {
  enum class EmitMode {
    kExecutable,
    kLLVMIR,
    kObjectFile,
  };

  EmitMode emit_mode = EmitMode::kExecutable;
  int arg_index = 1;
  if (argc >= 2) {
    const std::string first_arg = argv[1];
    if (first_arg == "-emit-llvm") {
      emit_mode = EmitMode::kLLVMIR;
      arg_index = 2;
    } else if (first_arg == "-emit-obj" || first_arg == "-emit-object") {
      emit_mode = EmitMode::kObjectFile;
      arg_index = 2;
    }
  }

  const int remaining_args = argc - arg_index;
  if (remaining_args < 1 || remaining_args > 2) {
    std::cerr
        << "Usage: " << argv[0]
        << " [-emit-llvm|-emit-obj|-emit-object] <input-file> [output]\n";
    return 1;
  }

  const std::string input_path = argv[arg_index];
  std::filesystem::path output_path;
  if (remaining_args == 2) {
    output_path = argv[arg_index + 1];
  } else {
    output_path = input_path;
    switch (emit_mode) {
      case EmitMode::kExecutable:
        output_path.replace_extension();
        break;
      case EmitMode::kLLVMIR:
        output_path.replace_extension(".ll");
        break;
      case EmitMode::kObjectFile:
        output_path.replace_extension(".o");
        break;
    }
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
        *debug_ctx);
    Parsing::CheckTypes(
        program,
        use_resolver,
        *debug_ctx);

    if (debug_ctx->GetErrors().HasErrors()) {
      debug_ctx->GetErrors().ThrowErrors();
    }

    Parsing::LLVMIRModule llvm_ir = Parsing::LowerToLLVMIRModule(
        program,
        use_resolver);
    switch (emit_mode) {
      case EmitMode::kExecutable:
        Parsing::LowerToExecutableFile(llvm_ir.GetModule(), output_path.string());
        break;
      case EmitMode::kLLVMIR: {
        std::ofstream output_stream(output_path);
        if (!output_stream) {
          throw std::runtime_error(
              "Failed to open output file: " + output_path.string());
        }
        output_stream << llvm_ir.ToString();
        break;
      }
      case EmitMode::kObjectFile:
        Parsing::LowerToObjectFile(llvm_ir.GetModule(), output_path.string());
        break;
    }
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
