#include "Lowering/LLVMConstructUtils.hpp"

#include <cassert>
#include <vector>

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include "Utils/Overload.hpp"

namespace Front {

LLVMConstructUtils::LLVMConstructUtils(
    llvm::LLVMContext& context,
    llvm::Module& module)
    : context_(context),
      module_(module) {}

llvm::Type* LLVMConstructUtils::BuildType(const Type* type) const {
  assert(type != nullptr);
  return std::visit(
      Utils::Overload{
          [this](const IntType&) -> llvm::Type* {
            return llvm::Type::getInt32Ty(context_);
          },
          [this](const BoolType&) -> llvm::Type* {
            return llvm::Type::getInt1Ty(context_);
          },
          [this](const ClassType&) -> llvm::Type* {
            return llvm::PointerType::getUnqual(context_);
          },
          [this](const FuncType&) -> llvm::Type* {
            return llvm::PointerType::getUnqual(context_);
          }},
      type->type);
}

llvm::Type* LLVMConstructUtils::BuildReturnType(
    const Type* type) const {
  if (type == nullptr) {
    return llvm::Type::getVoidTy(context_);
  }

  return BuildType(type);
}

llvm::FunctionType* LLVMConstructUtils::BuildFunctionType(
    const FunctionDeclarationStatement& function_declaration,
    const ClassDeclarationStatement* owner_class) const {
  std::vector<llvm::Type*> parameter_types;
  if (owner_class != nullptr) {
    parameter_types.push_back(llvm::PointerType::getUnqual(context_));
  }

  for (const FunctionParameter& parameter : function_declaration.parameters) {
    parameter_types.push_back(BuildType(parameter.type));
  }

  return llvm::FunctionType::get(
      BuildReturnType(
          AsFuncType(function_declaration.function_type) != nullptr
              ? AsFuncType(function_declaration.function_type)->return_type
              : nullptr),
      parameter_types,
      false);
}

llvm::Constant* LLVMConstructUtils::BuildDefaultConstant(const Type* type) const {
  assert(type != nullptr);
  return std::visit(
      Utils::Overload{
          [this](const IntType&) -> llvm::Constant* {
            return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 0);
          },
          [this](const BoolType&) -> llvm::Constant* {
            return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context_), false);
          },
          [this](const ClassType&) -> llvm::Constant* {
            return llvm::ConstantPointerNull::get(
                llvm::PointerType::getUnqual(context_));
          },
          [this](const FuncType&) -> llvm::Constant* {
            return llvm::ConstantPointerNull::get(
                llvm::PointerType::getUnqual(context_));
          }},
      type->type);
}

llvm::GlobalVariable* LLVMConstructUtils::CreateCStringGlobal(
    const std::string& name,
    const std::string& text) const {
  llvm::Constant* initializer =
      llvm::ConstantDataArray::getString(context_, text, true);
  auto* global = new llvm::GlobalVariable(
      module_,
      initializer->getType(),
      true,
      llvm::GlobalValue::PrivateLinkage,
      initializer,
      name);
  global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
  return global;
}

llvm::Constant* LLVMConstructUtils::BuildCStringPointer(
    llvm::GlobalVariable* string_global) const {
  llvm::Constant* zero =
      llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 0);
  llvm::Constant* indices[] = {zero, zero};
  return llvm::ConstantExpr::getInBoundsGetElementPtr(
      string_global->getValueType(),
      string_global,
      indices);
}

llvm::Constant* LLVMConstructUtils::BuildSizeOf(llvm::Type* type) const {
  llvm::Constant* size_ptr = llvm::ConstantExpr::getGetElementPtr(
      type,
      llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(context_)),
      llvm::ArrayRef<llvm::Constant*>{
          llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 1)});
  return llvm::ConstantExpr::getPtrToInt(
      size_ptr,
      llvm::Type::getInt64Ty(context_));
}

}  // namespace Front
