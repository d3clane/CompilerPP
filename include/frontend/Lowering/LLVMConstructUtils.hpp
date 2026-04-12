#pragma once

#include <string>

#include "Parsing/Ast.hpp"
#include "Parsing/Types.hpp"

namespace llvm {
class Constant;
class FunctionType;
class GlobalVariable;
class LLVMContext;
class Module;
class Type;
}  // namespace llvm

namespace Parsing {

class LLVMConstructUtils {
 public:
  LLVMConstructUtils(llvm::LLVMContext& context, llvm::Module& module);

  llvm::Type* BuildType(const Type* type) const;
  llvm::Type* BuildReturnType(const Type* type) const;
  llvm::FunctionType* BuildFunctionType(
      const FunctionDeclarationStatement& function_declaration,
      const ClassDeclarationStatement* owner_class) const;
  llvm::Constant* BuildDefaultConstant(const Type* type) const;

  llvm::GlobalVariable* CreateCStringGlobal(
      const std::string& name,
      const std::string& text) const;
  llvm::Constant* BuildCStringPointer(llvm::GlobalVariable* string_global) const;
  llvm::Constant* BuildSizeOf(llvm::Type* type) const;

 private:
  llvm::LLVMContext& context_;
  llvm::Module& module_;
};

}  // namespace Parsing
