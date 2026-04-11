#pragma once

#include <memory>
#include <string>

#include "Parsing/Ast.hpp"

namespace llvm {
class LLVMContext;
class Module;
}  // namespace llvm

namespace Parsing {

class TypeDefiner;
class UseResolver;

class LLVMIRModule {
 public:
  LLVMIRModule(
      std::unique_ptr<llvm::LLVMContext> context,
      std::unique_ptr<llvm::Module> module);
  ~LLVMIRModule();

  LLVMIRModule(LLVMIRModule&& other) noexcept;
  LLVMIRModule& operator=(LLVMIRModule&& other) noexcept;

  LLVMIRModule(const LLVMIRModule&) = delete;
  LLVMIRModule& operator=(const LLVMIRModule&) = delete;

  llvm::LLVMContext& GetContext();
  const llvm::LLVMContext& GetContext() const;
  llvm::Module& GetModule();
  const llvm::Module& GetModule() const;
  std::string ToString() const;

 private:
  std::unique_ptr<llvm::LLVMContext> context_;
  std::unique_ptr<llvm::Module> module_;
};

LLVMIRModule LowerToLLVMIRModule(
    const Program& program,
    const UseResolver& use_resolver,
    const TypeDefiner& type_definer);

std::string LowerToLLVMIR(
    const Program& program,
    const UseResolver& use_resolver,
    const TypeDefiner& type_definer);

}  // namespace Parsing
