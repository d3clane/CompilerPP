#include "Lowering/ObjectFileLowering.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

#include <llvm/ADT/StringMap.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/SubtargetFeature.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/AlwaysInliner.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Utils.h>

namespace Parsing {

namespace {

void InitializeNativeObjectTarget() {
  if (llvm::InitializeNativeTarget()) {
    throw std::runtime_error("Failed to initialize native LLVM target");
  }

  if (llvm::InitializeNativeTargetAsmPrinter()) {
    throw std::runtime_error("Failed to initialize native LLVM asm printer");
  }
}

std::unique_ptr<llvm::TargetMachine> CreateHostTargetMachine() {
  InitializeNativeObjectTarget();

  const std::string target_triple = llvm::sys::getDefaultTargetTriple();
  std::string lookup_error;
  const llvm::Target* target =
      llvm::TargetRegistry::lookupTarget(target_triple, lookup_error);
  if (target == nullptr) {
    throw std::runtime_error(
        "Failed to find LLVM target for triple " +
        target_triple +
        ": " +
        lookup_error);
  }

  llvm::TargetOptions target_options;
  const std::string cpu = llvm::sys::getHostCPUName().str();
  llvm::SubtargetFeatures host_features;
  for (const auto& feature : llvm::sys::getHostCPUFeatures()) {
    host_features.AddFeature(feature.getKey(), feature.getValue());
  }

  auto* target_machine = target->createTargetMachine(
      target_triple,
      cpu,
      host_features.getString(),
      target_options,
      llvm::Reloc::PIC_);
  if (target_machine == nullptr) {
    throw std::runtime_error(
        "Failed to create LLVM target machine for triple " + target_triple);
  }

  return std::unique_ptr<llvm::TargetMachine>(target_machine);
}

void RegisterStandardOptimizationPasses(
    llvm::legacy::PassManager& pass_manager) {
  pass_manager.add(llvm::createDeadArgEliminationPass());
  pass_manager.add(llvm::createPromoteMemoryToRegisterPass());
  pass_manager.add(llvm::createEarlyCSEPass());
  pass_manager.add(llvm::createInstructionCombiningPass());
  pass_manager.add(llvm::createReassociatePass());
  pass_manager.add(llvm::createGVNPass());
  pass_manager.add(llvm::createInstructionCombiningPass());
  pass_manager.add(llvm::createCFGSimplificationPass());
  pass_manager.add(llvm::createTailCallEliminationPass());
  pass_manager.add(llvm::createDeadCodeEliminationPass());
  pass_manager.add(llvm::createDeadArgEliminationPass());
}

}  // namespace

void LowerToObjectFile(
    llvm::Module& module,
    const std::string& output_path) {
  std::unique_ptr<llvm::TargetMachine> target_machine =
      CreateHostTargetMachine();
  module.setTargetTriple(target_machine->getTargetTriple().str());
  module.setDataLayout(target_machine->createDataLayout());

  std::error_code error_code;
  llvm::raw_fd_ostream destination(
      output_path,
      error_code,
      llvm::sys::fs::OF_None);
  if (error_code) {
    throw std::runtime_error(
        "Failed to open object output file " +
        output_path +
        ": " +
        error_code.message());
  }

  llvm::legacy::PassManager pass_manager;
  RegisterStandardOptimizationPasses(pass_manager);
  if (target_machine->addPassesToEmitFile(
          pass_manager,
          destination,
          nullptr,
          llvm::CodeGenFileType::ObjectFile)) {
    throw std::runtime_error(
        "LLVM target machine cannot emit an object file for " +
        target_machine->getTargetTriple().str());
  }

  pass_manager.run(module);
  destination.flush();
}

}  // namespace Parsing
