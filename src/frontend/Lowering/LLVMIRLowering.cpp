#include "Lowering/LLVMIRLowering.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include "Lowering/LLVMConstructUtils.hpp"
#include "Parsing/Types.hpp"
#include "SemanticAnalysis/ClassRelations.hpp"
#include "SemanticAnalysis/FunctionRelations.hpp"
#include "SemanticAnalysis/GlobalRelations.hpp"
#include "SemanticAnalysis/Resolver.hpp"
#include "SemanticAnalysis/StatementNumerizer.hpp"
#include "SemanticAnalysis/TypeDefiner.hpp"
#include "Utils/Overload.hpp"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/ErrorHandling.h"

namespace Parsing {

namespace {

std::string JoinStrings(
    const std::vector<std::string>& parts,
    const std::string& separator) {
  std::string result;
  bool is_first = true;
  for (const std::string& part : parts) {
    if (!is_first) {
      result += separator;
    }

    result += part;
    is_first = false;
  }

  return result;
}

class NodeNameBuilder {
 public:
  explicit NodeNameBuilder(const StatementNumerizer& numerizer)
      : numerizer_(numerizer) {}

  std::string BuildClassTypeName(
      const ClassDeclarationStatement& class_declaration) const {
    return BuildScopedDeclarationName(
        "class__" + class_declaration.class_name,
        &class_declaration);
  }

  std::string BuildClassVTableName(
      const ClassDeclarationStatement& class_declaration) const {
    return BuildScopedDeclarationName(
        "vtable__" + class_declaration.class_name,
        &class_declaration);
  }

  std::string BuildClassAllocatorName(
      const ClassDeclarationStatement& class_declaration) const {
    return BuildScopedDeclarationName(
        "new__" + class_declaration.class_name,
        &class_declaration);
  }

  std::string BuildClassDeallocatorName(
      const ClassDeclarationStatement& class_declaration) const {
    return BuildScopedDeclarationName(
        "delete__" + class_declaration.class_name,
        &class_declaration);
  }

  std::string BuildFunctionName(
      const FunctionDeclarationStatement& function_declaration,
      const ClassDeclarationStatement* owner_class) const {
    if (owner_class != nullptr) {
      const std::string base_name =
          "__" + owner_class->class_name + "__" +
          function_declaration.function_name;
      if (IsDeclaredInGlobalScope(owner_class)) {
        return base_name;
      }

      return base_name + "__" + BuildNodeSuffix(&function_declaration);
    }

    if (IsDeclaredInGlobalScope(&function_declaration) &&
        function_declaration.function_name != "main") {
      return function_declaration.function_name;
    }

    return function_declaration.function_name + "__" +
           BuildNodeSuffix(&function_declaration);
  }

  std::string BuildGlobalName(const DeclarationStatement& declaration) const {
    return BuildScopedDeclarationName(
        declaration.variable_name,
        &declaration);
  }

 private:
  StatementNumerizer::ScopedStmtRef GetNodeRef(const ASTNode* node) const {
    const std::optional<StatementNumerizer::ScopedStmtRef> ref =
        numerizer_.GetRef(node);
    assert(ref.has_value());
    return *ref;
  }

  bool IsDeclaredInGlobalScope(const ASTNode* node) const {
    const StatementNumerizer::ScopedStmtRef ref = GetNodeRef(node);
    if (ref.parent_node == nullptr) {
      return false;
    }

    const StatementNumerizer::ScopedStmtRef parent_ref =
        GetNodeRef(ref.parent_node);
    return parent_ref.parent_node == nullptr;
  }

  std::string BuildScopedDeclarationName(
      const std::string& base_name,
      const ASTNode* node) const {
    if (IsDeclaredInGlobalScope(node)) {
      return base_name;
    }

    return base_name + "__" + BuildNodeSuffix(node);
  }

  std::string BuildNodeSuffix(const ASTNode* node) const {
    std::vector<size_t> path;
    StatementNumerizer::ScopedStmtRef current_ref = GetNodeRef(node);
    while (true) {
      if (current_ref.stmt_id_in_scope != 0) {
        path.push_back(current_ref.stmt_id_in_scope);
      }

      if (current_ref.parent_node == nullptr) {
        break;
      }

      current_ref = GetNodeRef(current_ref.parent_node);
    }

    std::reverse(path.begin(), path.end());
    if (path.empty()) {
      return "root";
    }

    std::vector<std::string> string_path;
    string_path.reserve(path.size());
    for (size_t item : path) {
      string_path.push_back(std::to_string(item));
    }

    return JoinStrings(string_path, "_");
  }

  const StatementNumerizer& numerizer_;
};

struct FunctionInfo {
  const FunctionDeclarationStatement* declaration = nullptr;
  std::string ir_name;
};

struct ClassInfo {
  const ClassDeclarationStatement* declaration = nullptr;
  std::string type_name;
  std::string vtable_name;
  std::string allocator_name;
  std::string deallocator_name;
  std::vector<FuncType> slot_types;
  std::vector<const FunctionDeclarationStatement*> vtable_methods;
  std::map<std::string, size_t> slot_index_by_name;
};

struct PreparedState {
  StatementNumerizer numerizer;
  ClassRelations class_relations;
  FunctionRelations function_relations;
  GlobalRelations global_relations;
  std::map<const ASTNode*, std::string> global_name_by_node;
  std::map<const ClassDeclarationStatement*, ClassInfo> class_info_by_decl;
  std::map<const FunctionDeclarationStatement*, FunctionInfo> function_info_by_decl;
};

class PreparedStateBuilder {
 public:
  PreparedStateBuilder(
      const Program& program,
      const TypeDefiner& type_definer)
      : program_(program),
        type_definer_(type_definer) {}

  PreparedState Build() {
    PreparedState state;
    state.numerizer = BuildStatementNumerizer(program_);
    state.class_relations = BuildClassRelations(program_, type_definer_);
    state.function_relations = BuildFunctionRelations(program_);
    state.global_relations = BuildGlobalRelations(program_);

    NodeNameBuilder name_builder(state.numerizer);
    for (const ClassDeclarationStatement* class_declaration :
         state.class_relations.GetClassesInEncounterOrder()) {
      assert(class_declaration != nullptr);
      ClassInfo class_info;
      class_info.declaration = class_declaration;
      class_info.type_name = name_builder.BuildClassTypeName(*class_declaration);
      class_info.vtable_name = name_builder.BuildClassVTableName(*class_declaration);
      class_info.allocator_name =
          name_builder.BuildClassAllocatorName(*class_declaration);
      class_info.deallocator_name =
          name_builder.BuildClassDeallocatorName(*class_declaration);
      state.class_info_by_decl.emplace(class_declaration, std::move(class_info));
    }

    for (const FunctionDeclarationStatement* function_declaration :
         state.function_relations.GetFunctionsInEncounterOrder()) {
      assert(function_declaration != nullptr);
      FunctionInfo function_info;
      function_info.declaration = function_declaration;
      function_info.ir_name = name_builder.BuildFunctionName(
          *function_declaration,
          state.function_relations.GetOwnerClass(*function_declaration));
      state.function_info_by_decl.emplace(function_declaration, std::move(function_info));
    }

    for (const DeclarationStatement* declaration :
         state.global_relations.GetGlobalDeclarationsInEncounterOrder()) {
      assert(declaration != nullptr);
      state.global_name_by_node[declaration] =
          name_builder.BuildGlobalName(*declaration);
    }

    BuildClassInfo(state);
    return state;
  }

 private:
  void BuildClassInfo(PreparedState& state) {
    for (const ClassDeclarationStatement* class_declaration :
         state.class_relations.GetClassesBaseFirstOrder()) {
      assert(class_declaration != nullptr);

      ClassInfo& class_info = state.class_info_by_decl.at(class_declaration);
      const ClassDeclarationStatement* base_class =
          state.class_relations.GetBaseClass(*class_declaration);
      assert(!class_declaration->base_class_name.has_value() || base_class != nullptr);

      if (base_class != nullptr) {
        const ClassInfo& base_info = state.class_info_by_decl.at(base_class);
        class_info.slot_types = base_info.slot_types;
        class_info.vtable_methods = base_info.vtable_methods;
        class_info.slot_index_by_name = base_info.slot_index_by_name;
      }

      for (const FunctionDeclarationStatement& method : class_declaration->methods) {
        const FuncType method_type = BuildFunctionType(method);
        const auto inherited_slot_it =
            class_info.slot_index_by_name.find(method.function_name);
        if (inherited_slot_it == class_info.slot_index_by_name.end()) {
          const size_t slot_index = class_info.vtable_methods.size();
          class_info.slot_types.push_back(method_type);
          class_info.vtable_methods.push_back(&method);
          class_info.slot_index_by_name[method.function_name] = slot_index;
          continue;
        }

        const size_t slot_index = inherited_slot_it->second;
        assert(class_info.slot_types[slot_index] == method_type);

        class_info.vtable_methods[slot_index] = &method;
      }
    }
  }

  const Program& program_;
  const TypeDefiner& type_definer_;
};

class LLVMModuleBuilder;

struct LoweredValue {
  llvm::Value* value = nullptr;
  Type type;
};

struct StorageLocation {
  llvm::Value* pointer = nullptr;
  Type stored_type;
};

class FunctionLoweringContext {
 public:
  FunctionLoweringContext(
      LLVMModuleBuilder& owner,
      llvm::Function* function,
      const ClassDeclarationStatement* current_class_context,
      std::optional<Type> return_type);

  llvm::AllocaInst* CreateStackStorage(const Type& type);
  void BindStorage(const ASTNode* node, StorageLocation storage);
  void BindThis(llvm::Value* this_value);
  void PreallocateLocalDeclarations(const List<Statement>& statements);
  void LowerStatements(const List<Statement>& statements);
  void EmitDefaultReturnIfNeeded();
  void EmitClassDefaultInitializers(
      const ClassDeclarationStatement& class_declaration,
      llvm::Value* object_pointer);

  llvm::IRBuilder<>& Builder() {
    return builder_;
  }

  llvm::Function* Function() const {
    return function_;
  }

 private:
  friend class LLVMModuleBuilder;

  bool IsCurrentBlockTerminated() const;

  std::optional<StorageLocation> GetStorageForNode(const ASTNode* node) const;
  std::optional<Type> EvaluateExpressionType(const Expression& expression) const;
  const ClassDeclarationStatement* ResolveClassDeclaration(const Type& type) const;
  const ClassInfo& GetClassInfo(
      const ClassDeclarationStatement& class_declaration) const;

  llvm::Value* EmitLoad(llvm::Value* pointer, const Type& type);
  llvm::Value* EmitTypeAdjustedPointer(
      llvm::Value* value,
      const ClassDeclarationStatement& from_class,
      const ClassDeclarationStatement& to_class);
  LoweredValue AdjustValueToExpectedType(
      const Type& expected_type,
      LoweredValue value);
  llvm::Value* BuildFieldPointer(
      const ClassDeclarationStatement& owner_class,
      const DeclarationStatement& field_declaration,
      llvm::Value* object_pointer);
  llvm::Value* BuildVPtrFieldPointer(
      const ClassDeclarationStatement& current_class,
      llvm::Value* object_pointer);

  std::vector<LoweredValue> LowerCallArguments(
      const FunctionCall& function_call,
      const FunctionDeclarationStatement& function_declaration);
  std::optional<LoweredValue> LowerDirectFunctionCall(
      const FunctionCall& function_call,
      const FunctionDeclarationStatement& function_declaration);
  std::optional<LoweredValue> LowerVirtualCall(
      const ClassDeclarationStatement& static_class,
      llvm::Value* receiver_pointer,
      const FunctionCall& function_call,
      const FunctionDeclarationStatement& method_declaration);
  std::optional<LoweredValue> LowerIdentifierExpression(
      const IdentifierExpression& identifier_expression);
  std::optional<LoweredValue> LowerFunctionCall(
      const FunctionCall& function_call);
  std::optional<LoweredValue> LowerFieldAccess(const FieldAccess& field_access);
  std::optional<LoweredValue> LowerMethodCall(const MethodCall& method_call);
  std::optional<LoweredValue> LowerLogicalAnd(
      const LogicalAndExpression& expression);
  std::optional<LoweredValue> LowerLogicalOr(
      const LogicalOrExpression& expression);

  template <typename Node>
  std::optional<LoweredValue> LowerBinaryArithmetic(
      const Node& expression,
      llvm::Instruction::BinaryOps opcode);

  template <typename Node>
  std::optional<LoweredValue> LowerComparison(
      const Node& expression,
      llvm::CmpInst::Predicate predicate);
  std::optional<LoweredValue> LowerExpression(const Expression& expression);
  void StoreIntoPointer(
      llvm::Value* pointer,
      const Type& target_type,
      const Expression& expression);
  void LowerDeclaration(const DeclarationStatement& declaration);
  void LowerAssignment(const AssignmentStatement& assignment);
  void LowerPrint(const PrintStatement& print_statement);
  void LowerDelete(const DeleteStatement& delete_statement);
  void LowerReturn(const ReturnStatement& return_statement);
  void LowerElseTail(const ElseTail& else_tail);
  void LowerIf(const IfStatement& if_statement);
  void LowerStatement(const Statement& statement);
  void PreallocateFromStatement(const Statement& statement);
  void PreallocateFromElseTail(const ElseTail& else_tail);

  LLVMModuleBuilder& owner_;
  llvm::Function* function_;
  llvm::IRBuilder<> builder_;
  const ClassDeclarationStatement* current_class_context_ = nullptr;
  llvm::Value* current_this_value_ = nullptr;
  std::optional<Type> declared_return_type_;
  std::map<const ASTNode*, StorageLocation> storage_by_node_;
};

class LLVMModuleBuilder {
 public:
  LLVMModuleBuilder(
      const UseResolver& use_resolver,
      const TypeDefiner& type_definer,
      PreparedState state)
      : use_resolver_(use_resolver),
        type_definer_(type_definer),
        state_(std::move(state)),
        module_("CompilerPP", context_),
        llvm_utils_(context_, module_) {}

  std::string BuildIR() {
    DeclareRuntimeSupport();
    CreateClassTypes();
    CreateGlobalDeclarations();
    CreateFunctionDeclarations();
    CreateVTableGlobals();
    DefineAllocatorFunctions();
    DefineDeallocatorFunctions();
    DefineUserFunctions();
    DefineGlobalInitFunction();
    DefineMainWrapper();
    VerifyModule();

    std::string ir;
    llvm::raw_string_ostream stream(ir);
    module_.print(stream, nullptr);
    return stream.str();
  }

 private:
  friend class FunctionLoweringContext;

  struct LLVMClassData {
    llvm::StructType* type = nullptr;
    llvm::ArrayType* vtable_type = nullptr;
    llvm::Function* allocator = nullptr;
    llvm::Function* deallocator = nullptr;
    llvm::GlobalVariable* vtable = nullptr;
  };

  const ClassInfo& GetClassInfo(
      const ClassDeclarationStatement& class_declaration) const {
    return state_.class_info_by_decl.at(&class_declaration);
  }

  const FunctionInfo& GetFunctionInfo(
      const FunctionDeclarationStatement& function_declaration) const {
    return state_.function_info_by_decl.at(&function_declaration);
  }

  void DeclareRuntimeSupport() {
    calloc_function_ = llvm::Function::Create(
        llvm::FunctionType::get(
            llvm::PointerType::getUnqual(context_),
            {llvm::Type::getInt64Ty(context_), llvm::Type::getInt64Ty(context_)},
            false),
        llvm::GlobalValue::ExternalLinkage,
        "calloc",
        module_);
    free_function_ = llvm::Function::Create(
        llvm::FunctionType::get(
            llvm::Type::getVoidTy(context_),
            {llvm::PointerType::getUnqual(context_)},
            false),
        llvm::GlobalValue::ExternalLinkage,
        "free",
        module_);
    printf_function_ = llvm::Function::Create(
        llvm::FunctionType::get(
            llvm::Type::getInt32Ty(context_),
            {llvm::PointerType::getUnqual(context_)},
            true),
        llvm::GlobalValue::ExternalLinkage,
        "printf",
        module_);
    puts_function_ = llvm::Function::Create(
        llvm::FunctionType::get(
            llvm::Type::getInt32Ty(context_),
            {llvm::PointerType::getUnqual(context_)},
            false),
        llvm::GlobalValue::ExternalLinkage,
        "puts",
        module_);

    fmt_int_global_ = llvm_utils_.CreateCStringGlobal("fmt_int", "%d\n");
    fmt_ptr_global_ = llvm_utils_.CreateCStringGlobal("fmt_ptr", "%p\n");
    str_true_global_ = llvm_utils_.CreateCStringGlobal("str_true", "true");
    str_false_global_ = llvm_utils_.CreateCStringGlobal("str_false", "false");
  }

  void CreateClassTypes() {
    const auto& ordered_classes = state_.class_relations.GetClassesBaseFirstOrder();
    for (const ClassDeclarationStatement* class_declaration : ordered_classes) {
      assert(class_declaration != nullptr);
      const ClassInfo& class_info = GetClassInfo(*class_declaration);
      llvm_class_data_by_decl_[class_declaration].type =
          llvm::StructType::create(context_, class_info.type_name);
    }

    for (const ClassDeclarationStatement* class_declaration : ordered_classes) {
      assert(class_declaration != nullptr);

      const ClassInfo& class_info = GetClassInfo(*class_declaration);
      LLVMClassData& llvm_class_data = llvm_class_data_by_decl_.at(class_declaration);

      std::vector<llvm::Type*> elements;
      const ClassDeclarationStatement* base_class =
          state_.class_relations.GetBaseClass(*class_declaration);
      if (base_class != nullptr) {
        elements.push_back(llvm_class_data_by_decl_.at(base_class).type);
      } else {
        elements.push_back(llvm::PointerType::getUnqual(context_));
      }

      for (const DeclarationStatement& field : class_declaration->fields) {
        elements.push_back(llvm_utils_.BuildType(field.type));
      }

      llvm_class_data.type->setBody(elements);
      llvm_class_data.vtable_type =
          llvm::ArrayType::get(
              llvm::PointerType::getUnqual(context_),
              class_info.vtable_methods.size());
    }
  }

  void CreateGlobalDeclarations() {
    const auto& global_declarations =
        state_.global_relations.GetGlobalDeclarationsInEncounterOrder();
    for (const DeclarationStatement* declaration : global_declarations) {
      assert(declaration != nullptr);

      auto* global = new llvm::GlobalVariable(
          module_,
          llvm_utils_.BuildType(declaration->type),
          false,
          llvm::GlobalValue::InternalLinkage,
          llvm_utils_.BuildDefaultConstant(declaration->type),
          state_.global_name_by_node.at(declaration));
      llvm_global_by_decl_[declaration] = global;
    }
  }

  void CreateFunctionDeclarations() {
    const auto& ordered_functions =
        state_.function_relations.GetFunctionsInEncounterOrder();
    for (const FunctionDeclarationStatement* function_declaration :
         ordered_functions) {
      assert(function_declaration != nullptr);

      const FunctionInfo& function_info = GetFunctionInfo(*function_declaration);
      auto* function = llvm::Function::Create(
          llvm_utils_.BuildFunctionType(
              *function_declaration,
              state_.function_relations.GetOwnerClass(*function_declaration)),
          llvm::GlobalValue::ExternalLinkage,
          function_info.ir_name,
          module_);
      llvm_function_by_decl_[function_declaration] = function;
    }

    const auto& ordered_classes = state_.class_relations.GetClassesBaseFirstOrder();
    for (const ClassDeclarationStatement* class_declaration : ordered_classes) {
      assert(class_declaration != nullptr);

      const ClassInfo& class_info = GetClassInfo(*class_declaration);
      llvm_class_data_by_decl_.at(class_declaration).allocator = llvm::Function::Create(
          llvm::FunctionType::get(llvm::PointerType::getUnqual(context_), {}, false),
          llvm::GlobalValue::ExternalLinkage,
          class_info.allocator_name,
          module_);
      llvm_class_data_by_decl_.at(class_declaration).deallocator =
          llvm::Function::Create(
              llvm::FunctionType::get(
                  llvm::Type::getVoidTy(context_),
                  {llvm::PointerType::getUnqual(context_)},
                  false),
              llvm::GlobalValue::ExternalLinkage,
              class_info.deallocator_name,
              module_);
    }
  }

  void CreateVTableGlobals() {
    const auto& ordered_classes = state_.class_relations.GetClassesBaseFirstOrder();
    for (const ClassDeclarationStatement* class_declaration : ordered_classes) {
      assert(class_declaration != nullptr);

      const ClassInfo& class_info = GetClassInfo(*class_declaration);
      LLVMClassData& llvm_class_data = llvm_class_data_by_decl_.at(class_declaration);

      auto* vtable_global = new llvm::GlobalVariable(
          module_,
          llvm_class_data.vtable_type,
          true,
          llvm::GlobalValue::InternalLinkage,
          nullptr,
          class_info.vtable_name);
      vtable_global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
      llvm_class_data.vtable = vtable_global;

      std::vector<llvm::Constant*> entries;
      entries.reserve(class_info.vtable_methods.size());
      for (const FunctionDeclarationStatement* method : class_info.vtable_methods) {
        assert(method != nullptr);
        entries.push_back(llvm_function_by_decl_.at(method));
      }

      vtable_global->setInitializer(
          llvm::ConstantArray::get(llvm_class_data.vtable_type, entries));
    }
  }

  void DefineAllocatorFunctions() {
    const auto& ordered_classes = state_.class_relations.GetClassesBaseFirstOrder();
    for (const ClassDeclarationStatement* class_declaration : ordered_classes) {
      assert(class_declaration != nullptr);

      LLVMClassData& llvm_class_data = llvm_class_data_by_decl_.at(class_declaration);
      llvm::Function* allocator = llvm_class_data.allocator;

      auto* entry_block = llvm::BasicBlock::Create(context_, "entry", allocator);
      FunctionLoweringContext context(
          *this,
          allocator,
          class_declaration,
          Type{ClassType{class_declaration->class_name}});
      context.Builder().SetInsertPoint(entry_block);

      llvm::Value* allocated_object = context.Builder().CreateCall(
          calloc_function_,
          {
              llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 1),
              llvm_utils_.BuildSizeOf(llvm_class_data.type)});

      llvm::Value* vptr_field_ptr = context.BuildVPtrFieldPointer(
          *class_declaration,
          allocated_object);
      context.Builder().CreateStore(llvm_class_data.vtable, vptr_field_ptr);

      context.EmitClassDefaultInitializers(*class_declaration, allocated_object);
      context.Builder().CreateRet(allocated_object);
    }
  }

  void DefineDeallocatorFunctions() {
    const auto& ordered_classes = state_.class_relations.GetClassesBaseFirstOrder();
    for (const ClassDeclarationStatement* class_declaration : ordered_classes) {
      assert(class_declaration != nullptr);

      llvm::Function* deallocator =
          llvm_class_data_by_decl_.at(class_declaration).deallocator;
      auto* entry_block = llvm::BasicBlock::Create(context_, "entry", deallocator);
      llvm::IRBuilder<> builder(entry_block);

      llvm::Argument* object_pointer = &*deallocator->arg_begin();
      object_pointer->setName("__object");
      builder.CreateCall(free_function_, {object_pointer});
      builder.CreateRetVoid();
    }
  }

  void DefineUserFunctions() {
    const auto& ordered_functions =
        state_.function_relations.GetFunctionsInEncounterOrder();
    for (const FunctionDeclarationStatement* function_declaration :
         ordered_functions) {
      assert(function_declaration != nullptr);

      const ClassDeclarationStatement* owner_class =
          state_.function_relations.GetOwnerClass(*function_declaration);
      llvm::Function* function = llvm_function_by_decl_.at(function_declaration);
      auto* entry_block = llvm::BasicBlock::Create(context_, "entry", function);

      FunctionLoweringContext context(
          *this,
          function,
          owner_class,
          function_declaration->return_type);
      context.Builder().SetInsertPoint(entry_block);

      auto argument_it = function->arg_begin();
      if (owner_class != nullptr) {
        argument_it->setName("__this");
        context.BindThis(&*argument_it);
        ++argument_it;
      }

      std::vector<std::pair<llvm::Argument*, StorageLocation>> parameter_bindings;
      for (size_t parameter_i = 0;
           parameter_i < function_declaration->parameters.size();
           ++parameter_i, ++argument_it) {
        const FunctionParameter& parameter = function_declaration->parameters[parameter_i];
        StorageLocation storage{
            .pointer = context.CreateStackStorage(parameter.type),
            .stored_type = parameter.type};
        context.BindStorage(&parameter, storage);
        parameter_bindings.emplace_back(&*argument_it, storage);
      }

      assert(function_declaration->body != nullptr);
      context.PreallocateLocalDeclarations(function_declaration->body->statements);

      for (const auto& parameter_binding : parameter_bindings) {
        context.Builder().CreateStore(
            parameter_binding.first,
            parameter_binding.second.pointer);
      }

      context.LowerStatements(function_declaration->body->statements);
      context.EmitDefaultReturnIfNeeded();
    }
  }

  void DefineGlobalInitFunction() {
    global_init_function_ = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(context_), {}, false),
        llvm::GlobalValue::InternalLinkage,
        "__global_init",
        module_);

    auto* entry_block = llvm::BasicBlock::Create(context_, "entry", global_init_function_);
    FunctionLoweringContext context(*this, global_init_function_, nullptr, std::nullopt);
    context.Builder().SetInsertPoint(entry_block);

    const auto& global_declarations =
        state_.global_relations.GetGlobalDeclarationsInEncounterOrder();
    for (const DeclarationStatement* declaration : global_declarations) {
      assert(declaration != nullptr);

      llvm::GlobalVariable* global = llvm_global_by_decl_.at(declaration);
      if (declaration->initializer != nullptr) {
        context.StoreIntoPointer(
            global,
            declaration->type,
            *declaration->initializer);
        continue;
      }

      if (std::holds_alternative<ClassType>(declaration->type.type)) {
        const ClassDeclarationStatement* class_declaration =
            context.ResolveClassDeclaration(declaration->type);
        llvm::Value* allocated_object = context.Builder().CreateCall(
            llvm_class_data_by_decl_.at(class_declaration).allocator);
        context.Builder().CreateStore(allocated_object, global);
      }
    }

    context.Builder().CreateRetVoid();
  }

  void DefineMainWrapper() {
    const FunctionDeclarationStatement* user_main =
        state_.global_relations.GetUserMain();
    if (user_main == nullptr) {
      return;
    }

    assert(user_main->parameters.empty());

    auto* main_function = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getInt32Ty(context_), {}, false),
        llvm::GlobalValue::ExternalLinkage,
        "main",
        module_);
    auto* entry_block = llvm::BasicBlock::Create(context_, "entry", main_function);
    FunctionLoweringContext context(*this, main_function, nullptr, Type{IntType{}});
    context.Builder().SetInsertPoint(entry_block);

    context.Builder().CreateCall(global_init_function_);

    llvm::Function* user_main_function = llvm_function_by_decl_.at(user_main);
    if (!user_main->return_type.has_value()) {
      context.Builder().CreateCall(user_main_function);
      context.Builder().CreateRet(
          llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 0));
      return;
    }

    if (std::holds_alternative<IntType>(user_main->return_type->type)) {
      llvm::Value* result = context.Builder().CreateCall(user_main_function);
      context.Builder().CreateRet(result);
      return;
    }

    if (std::holds_alternative<BoolType>(user_main->return_type->type)) {
      llvm::Value* result = context.Builder().CreateCall(user_main_function);
      llvm::Value* extended_result =
          context.Builder().CreateZExt(result, llvm::Type::getInt32Ty(context_));
      context.Builder().CreateRet(extended_result);
      return;
    }

    llvm_unreachable("unreachable");
  }

  void VerifyModule() {
    std::string verify_errors;
    llvm::raw_string_ostream error_stream(verify_errors);
    if (llvm::verifyModule(module_, &error_stream)) {
      throw std::runtime_error(
          "LLVM module verification failed:\n" + error_stream.str());
    }
  }

  const UseResolver& use_resolver_;
  const TypeDefiner& type_definer_;
  PreparedState state_;

  mutable llvm::LLVMContext context_;
  llvm::Module module_;
  LLVMConstructUtils llvm_utils_;
  std::map<const ClassDeclarationStatement*, LLVMClassData> llvm_class_data_by_decl_;
  std::map<const FunctionDeclarationStatement*, llvm::Function*> llvm_function_by_decl_;
  std::map<const DeclarationStatement*, llvm::GlobalVariable*> llvm_global_by_decl_;

  llvm::Function* calloc_function_ = nullptr;
  llvm::Function* free_function_ = nullptr;
  llvm::Function* printf_function_ = nullptr;
  llvm::Function* puts_function_ = nullptr;
  llvm::Function* global_init_function_ = nullptr;

  llvm::GlobalVariable* fmt_int_global_ = nullptr;
  llvm::GlobalVariable* fmt_ptr_global_ = nullptr;
  llvm::GlobalVariable* str_true_global_ = nullptr;
  llvm::GlobalVariable* str_false_global_ = nullptr;
};

FunctionLoweringContext::FunctionLoweringContext(
    LLVMModuleBuilder& owner,
    llvm::Function* function,
    const ClassDeclarationStatement* current_class_context,
    std::optional<Type> return_type)
    : owner_(owner),
      function_(function),
      builder_(owner.context_),
      current_class_context_(current_class_context),
      declared_return_type_(std::move(return_type)) {}

llvm::AllocaInst* FunctionLoweringContext::CreateStackStorage(const Type& type) {
  return builder_.CreateAlloca(owner_.llvm_utils_.BuildType(type));
}

void FunctionLoweringContext::BindStorage(const ASTNode* node, StorageLocation storage) {
  storage_by_node_[node] = std::move(storage);
}

void FunctionLoweringContext::BindThis(llvm::Value* this_value) {
  current_this_value_ = this_value;
}

void FunctionLoweringContext::PreallocateLocalDeclarations(
    const List<Statement>& statements) {
  for (const auto& statement : statements) {
    assert(statement != nullptr);
    PreallocateFromStatement(*statement);
  }
}

void FunctionLoweringContext::PreallocateFromStatement(
    const Statement& statement) {
  std::visit(
      Utils::Overload{
          [this](const DeclarationStatement& declaration) {
            BindStorage(
                &declaration,
                StorageLocation{
                    .pointer = CreateStackStorage(declaration.type),
                    .stored_type = declaration.type});
          },
          [this](const IfStatement& if_statement) {
            if (if_statement.true_block != nullptr) {
              PreallocateLocalDeclarations(if_statement.true_block->statements);
            }

            if (if_statement.else_tail != nullptr) {
              PreallocateFromElseTail(*if_statement.else_tail);
            }
          },
          [this](const Block& block) {
            PreallocateLocalDeclarations(block.statements);
          },
          [](const auto&) {
          }},
      statement.value);
}

void FunctionLoweringContext::PreallocateFromElseTail(
    const ElseTail& else_tail) {
  if (else_tail.else_if != nullptr) {
    if (else_tail.else_if->true_block != nullptr) {
      PreallocateLocalDeclarations(else_tail.else_if->true_block->statements);
    }

    if (else_tail.else_if->else_tail != nullptr) {
      PreallocateFromElseTail(*else_tail.else_if->else_tail);
    }
    return;
  }

  if (else_tail.else_block != nullptr) {
    PreallocateLocalDeclarations(else_tail.else_block->statements);
  }
}

bool FunctionLoweringContext::IsCurrentBlockTerminated() const {
  return builder_.GetInsertBlock() != nullptr &&
         builder_.GetInsertBlock()->getTerminator() != nullptr;
}

std::optional<StorageLocation> FunctionLoweringContext::GetStorageForNode(
    const ASTNode* node) const {
  const auto storage_it = storage_by_node_.find(node);
  if (storage_it == storage_by_node_.end()) {
    return std::nullopt;
  }

  return storage_it->second;
}

const ClassInfo& FunctionLoweringContext::GetClassInfo(
    const ClassDeclarationStatement& class_declaration) const {
  return owner_.GetClassInfo(class_declaration);
}

const ClassDeclarationStatement* FunctionLoweringContext::ResolveClassDeclaration(
    const Type& type) const {
  const auto* class_type = std::get_if<ClassType>(&type.type);
  assert(class_type != nullptr);

  const ClassDeclarationStatement* class_declaration =
      owner_.type_definer_.GetClassDeclaration(class_type->class_name);
  assert(class_declaration != nullptr);

  return class_declaration;
}

std::optional<Type> FunctionLoweringContext::EvaluateExpressionType(
    const Expression& expression) const {
  return std::visit(
      Utils::Overload{
          [this](const IdentifierExpression& identifier_expression)
              -> std::optional<Type> {
            const UseResolver::ResolvedSymbol* resolved_symbol =
                owner_.use_resolver_.GetResolvedSymbol(
                    identifier_expression.name,
                    &identifier_expression);
            assert(resolved_symbol != nullptr);

            return resolved_symbol->symbol_data.type;
          },
          [](const LiteralExpression& literal_expression) -> std::optional<Type> {
            return std::visit(
                Utils::Overload{
                    [](int) -> std::optional<Type> {
                      return Type{IntType{}};
                    },
                    [](bool) -> std::optional<Type> {
                      return Type{BoolType{}};
                    }},
                literal_expression.value);
          },
          [this](const FunctionCall& function_call) -> std::optional<Type> {
            const UseResolver::ResolvedSymbol* resolved_symbol =
                owner_.use_resolver_.GetResolvedSymbol(
                    function_call.function_name,
                    &function_call);
            assert(resolved_symbol != nullptr);

            const auto* function_type =
                std::get_if<FuncType>(&resolved_symbol->symbol_data.type.type);
            assert(function_type != nullptr);

            if (function_type->return_type == nullptr) {
              return std::nullopt;
            }

            return *function_type->return_type;
          },
          [this](const MethodCall& method_call) -> std::optional<Type> {
            const UseResolver::ResolvedSymbol* resolved_symbol =
                owner_.use_resolver_.GetResolvedSymbol(
                    method_call.function_call.function_name,
                    &method_call.function_call);
            assert(resolved_symbol != nullptr);

            const auto* function_type =
                std::get_if<FuncType>(&resolved_symbol->symbol_data.type.type);
            assert(function_type != nullptr);

            if (function_type->return_type == nullptr) {
              return std::nullopt;
            }

            return *function_type->return_type;
          },
          [this](const FieldAccess& field_access) -> std::optional<Type> {
            const UseResolver::ResolvedSymbol* resolved_symbol =
                owner_.use_resolver_.GetResolvedSymbol(
                    field_access.field_name.name,
                    &field_access.field_name);
            assert(resolved_symbol != nullptr);

            return resolved_symbol->symbol_data.type;
          },
          [this](const UnaryPlusExpression& unary_expression)
              -> std::optional<Type> {
            assert(unary_expression.operand != nullptr);
            return EvaluateExpressionType(*unary_expression.operand);
          },
          [this](const UnaryMinusExpression& unary_expression)
              -> std::optional<Type> {
            assert(unary_expression.operand != nullptr);
            return EvaluateExpressionType(*unary_expression.operand);
          },
          [](const UnaryNotExpression&) -> std::optional<Type> {
            return Type{BoolType{}};
          },
          [](const AddExpression&) -> std::optional<Type> {
            return Type{IntType{}};
          },
          [](const SubtractExpression&) -> std::optional<Type> {
            return Type{IntType{}};
          },
          [](const MultiplyExpression&) -> std::optional<Type> {
            return Type{IntType{}};
          },
          [](const DivideExpression&) -> std::optional<Type> {
            return Type{IntType{}};
          },
          [](const ModuloExpression&) -> std::optional<Type> {
            return Type{IntType{}};
          },
          [](const LogicalAndExpression&) -> std::optional<Type> {
            return Type{BoolType{}};
          },
          [](const LogicalOrExpression&) -> std::optional<Type> {
            return Type{BoolType{}};
          },
          [](const EqualExpression&) -> std::optional<Type> {
            return Type{BoolType{}};
          },
          [](const NotEqualExpression&) -> std::optional<Type> {
            return Type{BoolType{}};
          },
          [](const LessExpression&) -> std::optional<Type> {
            return Type{BoolType{}};
          },
          [](const GreaterExpression&) -> std::optional<Type> {
            return Type{BoolType{}};
          },
          [](const LessEqualExpression&) -> std::optional<Type> {
            return Type{BoolType{}};
          },
          [](const GreaterEqualExpression&) -> std::optional<Type> {
            return Type{BoolType{}};
          }},
      expression.value);
}

llvm::Value* FunctionLoweringContext::EmitLoad(
    llvm::Value* pointer,
    const Type& type) {
  return builder_.CreateLoad(owner_.llvm_utils_.BuildType(type), pointer);
}

llvm::Value* FunctionLoweringContext::EmitTypeAdjustedPointer(
    llvm::Value* value,
    const ClassDeclarationStatement& from_class,
    const ClassDeclarationStatement& to_class) {
  if (&from_class == &to_class) {
    return value;
  }

  const ClassDeclarationStatement* current_class = &from_class;
  llvm::Value* current_value = value;
  while (current_class != nullptr && current_class != &to_class) {
    const ClassDeclarationStatement* base_class =
        owner_.state_.class_relations.GetBaseClass(*current_class);
    assert(base_class != nullptr);

    current_value = builder_.CreateStructGEP(
        owner_.llvm_class_data_by_decl_.at(current_class).type,
        current_value,
        0);
    current_class = base_class;
  }

  assert(current_class == &to_class);

  return current_value;
}

LoweredValue FunctionLoweringContext::AdjustValueToExpectedType(
    const Type& expected_type,
    LoweredValue value) {
  if (expected_type == value.type) {
    return value;
  }

  const auto* expected_class_type = std::get_if<ClassType>(&expected_type.type);
  const auto* actual_class_type = std::get_if<ClassType>(&value.type.type);
  if (expected_class_type == nullptr || actual_class_type == nullptr) {
    return value;
  }

  const ClassDeclarationStatement* expected_class =
      owner_.type_definer_.GetClassDeclaration(expected_class_type->class_name);
  const ClassDeclarationStatement* actual_class =
      owner_.type_definer_.GetClassDeclaration(actual_class_type->class_name);
  assert(expected_class != nullptr);
  assert(actual_class != nullptr);

  value.value = EmitTypeAdjustedPointer(
      value.value,
      *actual_class,
      *expected_class);
  value.type = expected_type;
  return value;
}

llvm::Value* FunctionLoweringContext::BuildFieldPointer(
    const ClassDeclarationStatement& owner_class,
    const DeclarationStatement& field_declaration,
    llvm::Value* object_pointer) {
  const std::optional<size_t> field_index =
      owner_.state_.class_relations.GetLocalFieldIndex(&field_declaration);
  assert(field_index.has_value());

  return builder_.CreateStructGEP(
      owner_.llvm_class_data_by_decl_.at(&owner_class).type,
      object_pointer,
      *field_index + 1);
}

llvm::Value* FunctionLoweringContext::BuildVPtrFieldPointer(
    const ClassDeclarationStatement& current_class,
    llvm::Value* object_pointer) {
  const ClassDeclarationStatement* iter_class = &current_class;
  llvm::Value* current_pointer = object_pointer;
  while (iter_class != nullptr) {
    const ClassDeclarationStatement* base_class =
        owner_.state_.class_relations.GetBaseClass(*iter_class);

    current_pointer = builder_.CreateStructGEP(
        owner_.llvm_class_data_by_decl_.at(iter_class).type,
        current_pointer,
        0);
    iter_class = base_class;
  }

  return current_pointer;
}

std::vector<LoweredValue> FunctionLoweringContext::LowerCallArguments(
    const FunctionCall& function_call,
    const FunctionDeclarationStatement& function_declaration) {
  std::vector<std::optional<LoweredValue>> ordered_arguments(
      function_declaration.parameters.size());
  size_t positional_index = 0;

  for (const auto& argument : function_call.arguments) {
    assert(argument != nullptr);
    std::visit(
        Utils::Overload{
            [this,
             &ordered_arguments,
             &function_declaration](const NamedCallArgument& named_argument) {
              size_t parameter_index = function_declaration.parameters.size();
              for (size_t param_i = 0;
                   param_i < function_declaration.parameters.size();
                   ++param_i) {
                if (function_declaration.parameters[param_i].name == named_argument.name) {
                  parameter_index = param_i;
                  break;
                }
              }

              assert(parameter_index != function_declaration.parameters.size());

              assert(named_argument.value != nullptr);
              const std::optional<LoweredValue> lowered_argument =
                  LowerExpression(*named_argument.value);
              assert(lowered_argument.has_value());

              ordered_arguments[parameter_index] = AdjustValueToExpectedType(
                  function_declaration.parameters[parameter_index].type,
                  *lowered_argument);
            },
            [this,
             &ordered_arguments,
             &positional_index,
             &function_declaration](const PositionalCallArgument& positional_argument) {
              assert(positional_index < function_declaration.parameters.size());

              assert(positional_argument.value != nullptr);
              const std::optional<LoweredValue> lowered_argument =
                  LowerExpression(*positional_argument.value);
              assert(lowered_argument.has_value());

              ordered_arguments[positional_index] = AdjustValueToExpectedType(
                  function_declaration.parameters[positional_index].type,
                  *lowered_argument);
              ++positional_index;
            }},
        *argument);
  }

  std::vector<LoweredValue> result;
  result.reserve(ordered_arguments.size());
  for (const auto& ordered_argument : ordered_arguments) {
    assert(ordered_argument.has_value());
    result.push_back(*ordered_argument);
  }

  return result;
}

std::optional<LoweredValue> FunctionLoweringContext::LowerDirectFunctionCall(
    const FunctionCall& function_call,
    const FunctionDeclarationStatement& function_declaration) {
  llvm::Function* function = owner_.llvm_function_by_decl_.at(&function_declaration);
  const std::vector<LoweredValue> lowered_arguments =
      LowerCallArguments(function_call, function_declaration);

  std::vector<llvm::Value*> call_arguments;
  call_arguments.reserve(lowered_arguments.size());
  for (const LoweredValue& lowered_argument : lowered_arguments) {
    call_arguments.push_back(lowered_argument.value);
  }

  llvm::CallInst* call = builder_.CreateCall(function, call_arguments);
  if (!function_declaration.return_type.has_value()) {
    return std::nullopt;
  }

  return LoweredValue{.value = call, .type = *function_declaration.return_type};
}

std::optional<LoweredValue> FunctionLoweringContext::LowerVirtualCall(
    const ClassDeclarationStatement& static_class,
    llvm::Value* receiver_pointer,
    const FunctionCall& function_call,
    const FunctionDeclarationStatement& method_declaration) {
  const ClassInfo& class_info = GetClassInfo(static_class);
  const auto slot_it =
      class_info.slot_index_by_name.find(method_declaration.function_name);
  assert(slot_it != class_info.slot_index_by_name.end());

  const std::vector<LoweredValue> lowered_arguments =
      LowerCallArguments(function_call, method_declaration);

  llvm::Value* vptr_field_ptr = BuildVPtrFieldPointer(
      static_class,
      receiver_pointer);
  llvm::Value* vtable_ptr = builder_.CreateLoad(
      llvm::PointerType::getUnqual(owner_.context_),
      vptr_field_ptr);

  const LLVMModuleBuilder::LLVMClassData& llvm_class_data =
      owner_.llvm_class_data_by_decl_.at(&static_class);
  llvm::Value* slot_indices[] = {
      llvm::ConstantInt::get(llvm::Type::getInt32Ty(owner_.context_), 0),
      llvm::ConstantInt::get(
          llvm::Type::getInt32Ty(owner_.context_),
          static_cast<int>(slot_it->second))};
  llvm::Value* slot_ptr = builder_.CreateInBoundsGEP(
      llvm_class_data.vtable_type,
      vtable_ptr,
      slot_indices);
  llvm::Value* function_ptr = builder_.CreateLoad(
      llvm::PointerType::getUnqual(owner_.context_),
      slot_ptr);

  std::vector<llvm::Value*> call_arguments;
  call_arguments.reserve(lowered_arguments.size() + 1);
  call_arguments.push_back(receiver_pointer);
  for (const LoweredValue& lowered_argument : lowered_arguments) {
    call_arguments.push_back(lowered_argument.value);
  }

  llvm::FunctionType* function_type = owner_.llvm_utils_.BuildFunctionType(
      method_declaration,
      owner_.state_.function_relations.GetOwnerClass(method_declaration));
  llvm::CallInst* call = builder_.CreateCall(function_type, function_ptr, call_arguments);
  if (!method_declaration.return_type.has_value()) {
    return std::nullopt;
  }

  return LoweredValue{.value = call, .type = *method_declaration.return_type};
}

std::optional<LoweredValue> FunctionLoweringContext::LowerIdentifierExpression(
    const IdentifierExpression& identifier_expression) {
  const UseResolver::ResolvedSymbol* resolved_symbol =
      owner_.use_resolver_.GetResolvedSymbol(
          identifier_expression.name,
          &identifier_expression);
  assert(resolved_symbol != nullptr);

  if (resolved_symbol->symbol_data.kind == SymbolKind::Function) {
    const auto* function_declaration =
        static_cast<const FunctionDeclarationStatement*>(resolved_symbol->definition_node);
    return LoweredValue{
        .value = owner_.llvm_function_by_decl_.at(function_declaration),
        .type = resolved_symbol->symbol_data.type};
  }

  if (const ClassDeclarationStatement* owner_class =
          owner_.state_.class_relations.GetFieldOwner(resolved_symbol->definition_node);
      owner_class != nullptr) {
    assert(current_this_value_ != nullptr);

    const auto* field_declaration =
        static_cast<const DeclarationStatement*>(resolved_symbol->definition_node);
    llvm::Value* field_ptr = BuildFieldPointer(
        *owner_class,
        *field_declaration,
        current_this_value_);
    return LoweredValue{
        .value = EmitLoad(field_ptr, field_declaration->type),
        .type = field_declaration->type};
  }

  const std::optional<StorageLocation> storage =
      GetStorageForNode(resolved_symbol->definition_node);
  if (storage.has_value()) {
    return LoweredValue{
        .value = EmitLoad(storage->pointer, storage->stored_type),
        .type = storage->stored_type};
  }

  const auto global_it =
      owner_.llvm_global_by_decl_.find(
          static_cast<const DeclarationStatement*>(resolved_symbol->definition_node));
  if (global_it != owner_.llvm_global_by_decl_.end()) {
    return LoweredValue{
        .value = EmitLoad(global_it->second, resolved_symbol->symbol_data.type),
        .type = resolved_symbol->symbol_data.type};
  }

  llvm_unreachable("identifier storage must exist before lowering");
}

std::optional<LoweredValue> FunctionLoweringContext::LowerFunctionCall(
    const FunctionCall& function_call) {
  const UseResolver::ResolvedSymbol* resolved_symbol =
      owner_.use_resolver_.GetResolvedSymbol(
          function_call.function_name,
          &function_call);
  assert(resolved_symbol != nullptr);

  assert(resolved_symbol->symbol_data.kind == SymbolKind::Function);

  const auto* function_declaration =
      static_cast<const FunctionDeclarationStatement*>(resolved_symbol->definition_node);
  const ClassDeclarationStatement* owner_class =
      owner_.state_.function_relations.GetOwnerClass(*function_declaration);
  if (owner_class == nullptr) {
    return LowerDirectFunctionCall(function_call, *function_declaration);
  }

  assert(current_this_value_ != nullptr);
  assert(current_class_context_ != nullptr);

  return LowerVirtualCall(
      *current_class_context_,
      current_this_value_,
      function_call,
      *function_declaration);
}

std::optional<LoweredValue> FunctionLoweringContext::LowerFieldAccess(
    const FieldAccess& field_access) {
  const std::optional<LoweredValue> receiver_value =
      LowerIdentifierExpression(field_access.object_name);
  assert(receiver_value.has_value());

  const UseResolver::ResolvedSymbol* resolved_field =
      owner_.use_resolver_.GetResolvedSymbol(
          field_access.field_name.name,
          &field_access.field_name);
  assert(resolved_field != nullptr);

  assert(resolved_field->declaring_class != nullptr);

  const auto* field_declaration =
      static_cast<const DeclarationStatement*>(resolved_field->definition_node);
  llvm::Value* field_ptr = BuildFieldPointer(
      *resolved_field->declaring_class,
      *field_declaration,
      receiver_value->value);
  return LoweredValue{
      .value = EmitLoad(field_ptr, field_declaration->type),
      .type = field_declaration->type};
}

std::optional<LoweredValue> FunctionLoweringContext::LowerMethodCall(
    const MethodCall& method_call) {
  const std::optional<LoweredValue> receiver_value =
      LowerIdentifierExpression(method_call.object_name);
  assert(receiver_value.has_value());

  const ClassDeclarationStatement* receiver_class =
      ResolveClassDeclaration(receiver_value->type);
  const UseResolver::ResolvedSymbol* resolved_method =
      owner_.use_resolver_.GetResolvedSymbol(
          method_call.function_call.function_name,
          &method_call.function_call);
  assert(resolved_method != nullptr);

  const auto* function_declaration =
      static_cast<const FunctionDeclarationStatement*>(resolved_method->definition_node);
  return LowerVirtualCall(
      *receiver_class,
      receiver_value->value,
      method_call.function_call,
      *function_declaration);
}

std::optional<LoweredValue> FunctionLoweringContext::LowerLogicalAnd(
    const LogicalAndExpression& expression) {
  assert(expression.left != nullptr);
  assert(expression.right != nullptr);
  const std::optional<LoweredValue> left = LowerExpression(*expression.left);
  assert(left.has_value());

  auto* function = builder_.GetInsertBlock()->getParent();
  auto* rhs_block = llvm::BasicBlock::Create(owner_.context_, "land.rhs", function);
  auto* false_block = llvm::BasicBlock::Create(owner_.context_, "land.false", function);
  auto* merge_block = llvm::BasicBlock::Create(owner_.context_, "land.merge");
  builder_.CreateCondBr(left->value, rhs_block, false_block);

  builder_.SetInsertPoint(rhs_block);
  const std::optional<LoweredValue> right = LowerExpression(*expression.right);
  assert(right.has_value());
  llvm::BasicBlock* rhs_exit = builder_.GetInsertBlock();
  builder_.CreateBr(merge_block);

  builder_.SetInsertPoint(false_block);
  llvm::BasicBlock* false_exit = builder_.GetInsertBlock();
  builder_.CreateBr(merge_block);

  function->insert(function->end(), merge_block);
  builder_.SetInsertPoint(merge_block);
  auto* result = builder_.CreatePHI(llvm::Type::getInt1Ty(owner_.context_), 2);
  result->addIncoming(
      llvm::ConstantInt::get(llvm::Type::getInt1Ty(owner_.context_), false),
      false_exit);
  result->addIncoming(right->value, rhs_exit);
  return LoweredValue{.value = result, .type = Type{BoolType{}}};
}

std::optional<LoweredValue> FunctionLoweringContext::LowerLogicalOr(
    const LogicalOrExpression& expression) {
  assert(expression.left != nullptr);
  assert(expression.right != nullptr);
  const std::optional<LoweredValue> left = LowerExpression(*expression.left);
  assert(left.has_value());

  auto* function = builder_.GetInsertBlock()->getParent();
  auto* rhs_block = llvm::BasicBlock::Create(owner_.context_, "lor.rhs", function);
  auto* true_block = llvm::BasicBlock::Create(owner_.context_, "lor.true", function);
  auto* merge_block = llvm::BasicBlock::Create(owner_.context_, "lor.merge");
  builder_.CreateCondBr(left->value, true_block, rhs_block);

  builder_.SetInsertPoint(true_block);
  llvm::BasicBlock* true_exit = builder_.GetInsertBlock();
  builder_.CreateBr(merge_block);

  builder_.SetInsertPoint(rhs_block);
  const std::optional<LoweredValue> right = LowerExpression(*expression.right);
  assert(right.has_value());
  llvm::BasicBlock* rhs_exit = builder_.GetInsertBlock();
  builder_.CreateBr(merge_block);

  function->insert(function->end(), merge_block);
  builder_.SetInsertPoint(merge_block);
  auto* result = builder_.CreatePHI(llvm::Type::getInt1Ty(owner_.context_), 2);
  result->addIncoming(
      llvm::ConstantInt::get(llvm::Type::getInt1Ty(owner_.context_), true),
      true_exit);
  result->addIncoming(right->value, rhs_exit);
  return LoweredValue{.value = result, .type = Type{BoolType{}}};
}

template <typename Node>
std::optional<LoweredValue> FunctionLoweringContext::LowerBinaryArithmetic(
    const Node& expression,
    llvm::Instruction::BinaryOps opcode) {
  assert(expression.left != nullptr);
  assert(expression.right != nullptr);
  const std::optional<LoweredValue> left = LowerExpression(*expression.left);
  const std::optional<LoweredValue> right = LowerExpression(*expression.right);
  assert(left.has_value());
  assert(right.has_value());

  return LoweredValue{
      .value = builder_.CreateBinOp(opcode, left->value, right->value),
      .type = Type{IntType{}}};
}

template <typename Node>
std::optional<LoweredValue> FunctionLoweringContext::LowerComparison(
    const Node& expression,
    llvm::CmpInst::Predicate predicate) {
  assert(expression.left != nullptr);
  assert(expression.right != nullptr);
  const std::optional<LoweredValue> left = LowerExpression(*expression.left);
  const std::optional<LoweredValue> right = LowerExpression(*expression.right);
  assert(left.has_value());
  assert(right.has_value());

  return LoweredValue{
      .value = builder_.CreateICmp(predicate, left->value, right->value),
      .type = Type{BoolType{}}};
}

std::optional<LoweredValue> FunctionLoweringContext::LowerExpression(
    const Expression& expression) {
  return std::visit(
      Utils::Overload{
          [this](const IdentifierExpression& identifier_expression)
              -> std::optional<LoweredValue> {
            return LowerIdentifierExpression(identifier_expression);
          },
          [this](const LiteralExpression& literal_expression)
              -> std::optional<LoweredValue> {
            return std::visit(
                Utils::Overload{
                    [this](int value) -> std::optional<LoweredValue> {
                      return LoweredValue{
                          .value = llvm::ConstantInt::get(
                              llvm::Type::getInt32Ty(owner_.context_),
                              value),
                          .type = Type{IntType{}}};
                    },
                    [this](bool value) -> std::optional<LoweredValue> {
                      return LoweredValue{
                          .value = llvm::ConstantInt::get(
                              llvm::Type::getInt1Ty(owner_.context_),
                              value),
                          .type = Type{BoolType{}}};
                    }},
                literal_expression.value);
          },
          [this](const UnaryPlusExpression& unary_expression)
              -> std::optional<LoweredValue> {
            assert(unary_expression.operand != nullptr);
            return LowerExpression(*unary_expression.operand);
          },
          [this](const UnaryMinusExpression& unary_expression)
              -> std::optional<LoweredValue> {
            assert(unary_expression.operand != nullptr);
            const std::optional<LoweredValue> operand =
                LowerExpression(*unary_expression.operand);
            assert(operand.has_value());

            return LoweredValue{
                .value = builder_.CreateSub(
                    llvm::ConstantInt::get(
                        llvm::Type::getInt32Ty(owner_.context_),
                        0),
                    operand->value),
                .type = Type{IntType{}}};
          },
          [this](const UnaryNotExpression& unary_expression)
              -> std::optional<LoweredValue> {
            assert(unary_expression.operand != nullptr);
            const std::optional<LoweredValue> operand =
                LowerExpression(*unary_expression.operand);
            assert(operand.has_value());

            return LoweredValue{
                .value = builder_.CreateXor(
                    operand->value,
                    llvm::ConstantInt::get(
                        llvm::Type::getInt1Ty(owner_.context_),
                        true)),
                .type = Type{BoolType{}}};
          },
          [this](const AddExpression& node) -> std::optional<LoweredValue> {
            return LowerBinaryArithmetic(node, llvm::Instruction::Add);
          },
          [this](const SubtractExpression& node) -> std::optional<LoweredValue> {
            return LowerBinaryArithmetic(node, llvm::Instruction::Sub);
          },
          [this](const MultiplyExpression& node) -> std::optional<LoweredValue> {
            return LowerBinaryArithmetic(node, llvm::Instruction::Mul);
          },
          [this](const DivideExpression& node) -> std::optional<LoweredValue> {
            return LowerBinaryArithmetic(node, llvm::Instruction::SDiv);
          },
          [this](const ModuloExpression& node) -> std::optional<LoweredValue> {
            return LowerBinaryArithmetic(node, llvm::Instruction::SRem);
          },
          [this](const LogicalAndExpression& node) -> std::optional<LoweredValue> {
            return LowerLogicalAnd(node);
          },
          [this](const LogicalOrExpression& node) -> std::optional<LoweredValue> {
            return LowerLogicalOr(node);
          },
          [this](const EqualExpression& node) -> std::optional<LoweredValue> {
            return LowerComparison(node, llvm::CmpInst::ICMP_EQ);
          },
          [this](const NotEqualExpression& node) -> std::optional<LoweredValue> {
            return LowerComparison(node, llvm::CmpInst::ICMP_NE);
          },
          [this](const LessExpression& node) -> std::optional<LoweredValue> {
            return LowerComparison(node, llvm::CmpInst::ICMP_SLT);
          },
          [this](const GreaterExpression& node) -> std::optional<LoweredValue> {
            return LowerComparison(node, llvm::CmpInst::ICMP_SGT);
          },
          [this](const LessEqualExpression& node) -> std::optional<LoweredValue> {
            return LowerComparison(node, llvm::CmpInst::ICMP_SLE);
          },
          [this](const GreaterEqualExpression& node) -> std::optional<LoweredValue> {
            return LowerComparison(node, llvm::CmpInst::ICMP_SGE);
          },
          [this](const FunctionCall& function_call) -> std::optional<LoweredValue> {
            return LowerFunctionCall(function_call);
          },
          [this](const FieldAccess& field_access) -> std::optional<LoweredValue> {
            return LowerFieldAccess(field_access);
          },
          [this](const MethodCall& method_call) -> std::optional<LoweredValue> {
            return LowerMethodCall(method_call);
          }},
      expression.value);
}

void FunctionLoweringContext::StoreIntoPointer(
    llvm::Value* pointer,
    const Type& target_type,
    const Expression& expression) {
  const std::optional<LoweredValue> lowered_value = LowerExpression(expression);
  assert(lowered_value.has_value());

  const LoweredValue adjusted_value =
      AdjustValueToExpectedType(target_type, *lowered_value);
  builder_.CreateStore(adjusted_value.value, pointer);
}

void FunctionLoweringContext::LowerDeclaration(const DeclarationStatement& declaration) {
  const auto storage_it = storage_by_node_.find(&declaration);
  assert(storage_it != storage_by_node_.end());

  if (declaration.initializer != nullptr) {
    StoreIntoPointer(
        storage_it->second.pointer,
        declaration.type,
        *declaration.initializer);
    return;
  }

  if (std::holds_alternative<ClassType>(declaration.type.type)) {
    const ClassDeclarationStatement* class_declaration =
        ResolveClassDeclaration(declaration.type);
    llvm::Value* allocated_object = builder_.CreateCall(
        owner_.llvm_class_data_by_decl_.at(class_declaration).allocator);
    builder_.CreateStore(allocated_object, storage_it->second.pointer);
    return;
  }

  builder_.CreateStore(
      owner_.llvm_utils_.BuildDefaultConstant(declaration.type),
      storage_it->second.pointer);
}

void FunctionLoweringContext::LowerAssignment(const AssignmentStatement& assignment) {
  const UseResolver::ResolvedSymbol* resolved_symbol =
      owner_.use_resolver_.GetResolvedSymbol(
          assignment.variable_name,
          &assignment);
  assert(resolved_symbol != nullptr);

  llvm::Value* target_pointer = nullptr;
  Type target_type = resolved_symbol->symbol_data.type;
  if (const ClassDeclarationStatement* owner_class =
          owner_.state_.class_relations.GetFieldOwner(resolved_symbol->definition_node);
      owner_class != nullptr) {
    assert(current_this_value_ != nullptr);

    const auto* field_declaration =
        static_cast<const DeclarationStatement*>(resolved_symbol->definition_node);
    target_pointer = BuildFieldPointer(
        *owner_class,
        *field_declaration,
        current_this_value_);
    target_type = field_declaration->type;
  } else if (const std::optional<StorageLocation> storage =
                 GetStorageForNode(resolved_symbol->definition_node);
             storage.has_value()) {
    target_pointer = storage->pointer;
    target_type = storage->stored_type;
  } else if (const auto global_it =
                 owner_.llvm_global_by_decl_.find(
                     static_cast<const DeclarationStatement*>(
                         resolved_symbol->definition_node));
             global_it != owner_.llvm_global_by_decl_.end()) {
    target_pointer = global_it->second;
  } else {
    assert(false && "assignment target must resolve before lowering");
  }

  assert(assignment.expr != nullptr);
  StoreIntoPointer(target_pointer, target_type, *assignment.expr);
}

void FunctionLoweringContext::LowerPrint(const PrintStatement& print_statement) {
  assert(print_statement.expr != nullptr);
  const std::optional<LoweredValue> lowered_value =
      LowerExpression(*print_statement.expr);
  assert(lowered_value.has_value());

  if (std::holds_alternative<IntType>(lowered_value->type.type)) {
    builder_.CreateCall(
        owner_.printf_function_,
        {
            owner_.llvm_utils_.BuildCStringPointer(owner_.fmt_int_global_),
            lowered_value->value});
    return;
  }

  if (std::holds_alternative<BoolType>(lowered_value->type.type)) {
    llvm::Value* selected_ptr = builder_.CreateSelect(
        lowered_value->value,
        owner_.llvm_utils_.BuildCStringPointer(owner_.str_true_global_),
        owner_.llvm_utils_.BuildCStringPointer(owner_.str_false_global_));
    builder_.CreateCall(owner_.puts_function_, {selected_ptr});
    return;
  }

  builder_.CreateCall(
      owner_.printf_function_,
      {
          owner_.llvm_utils_.BuildCStringPointer(owner_.fmt_ptr_global_),
          lowered_value->value});
}

void FunctionLoweringContext::LowerDelete(const DeleteStatement& delete_statement) {
  const UseResolver::ResolvedSymbol* resolved_symbol =
      owner_.use_resolver_.GetResolvedSymbol(
          delete_statement.variable.name,
          &delete_statement.variable);
  assert(resolved_symbol != nullptr);

  llvm::Value* target_pointer = nullptr;
  Type target_type = resolved_symbol->symbol_data.type;
  if (const ClassDeclarationStatement* owner_class =
          owner_.state_.class_relations.GetFieldOwner(resolved_symbol->definition_node);
      owner_class != nullptr) {
    assert(current_this_value_ != nullptr);

    const auto* field_declaration =
        static_cast<const DeclarationStatement*>(resolved_symbol->definition_node);
    llvm::Value* field_ptr = BuildFieldPointer(
        *owner_class,
        *field_declaration,
        current_this_value_);
    target_pointer = EmitLoad(field_ptr, field_declaration->type);
    target_type = field_declaration->type;
  } else if (const std::optional<StorageLocation> storage =
                 GetStorageForNode(resolved_symbol->definition_node);
             storage.has_value()) {
    target_pointer = EmitLoad(storage->pointer, storage->stored_type);
    target_type = storage->stored_type;
  } else if (const auto global_it =
                 owner_.llvm_global_by_decl_.find(
                     static_cast<const DeclarationStatement*>(
                         resolved_symbol->definition_node));
             global_it != owner_.llvm_global_by_decl_.end()) {
    target_pointer = EmitLoad(global_it->second, resolved_symbol->symbol_data.type);
  } else {
    assert(false && "delete target must resolve before lowering");
  }

  const ClassDeclarationStatement* class_declaration =
      ResolveClassDeclaration(target_type);
  builder_.CreateCall(
      owner_.llvm_class_data_by_decl_.at(class_declaration).deallocator,
      {target_pointer});
}

void FunctionLoweringContext::LowerReturn(const ReturnStatement& return_statement) {
  if (!declared_return_type_.has_value()) {
    builder_.CreateRetVoid();
    return;
  }

  assert(return_statement.expr != nullptr);

  const std::optional<LoweredValue> lowered_value =
      LowerExpression(*return_statement.expr);
  assert(lowered_value.has_value());

  const LoweredValue adjusted_value = AdjustValueToExpectedType(
      *declared_return_type_,
      *lowered_value);
  builder_.CreateRet(adjusted_value.value);
}

void FunctionLoweringContext::LowerElseTail(const ElseTail& else_tail) {
  if (else_tail.else_if != nullptr) {
    LowerIf(*else_tail.else_if);
    return;
  }

  if (else_tail.else_block != nullptr) {
    LowerStatements(else_tail.else_block->statements);
  }
}

void FunctionLoweringContext::LowerIf(const IfStatement& if_statement) {
  assert(if_statement.condition != nullptr);
  assert(if_statement.true_block != nullptr);

  const std::optional<LoweredValue> condition =
      LowerExpression(*if_statement.condition);
  assert(condition.has_value());

  auto* function = builder_.GetInsertBlock()->getParent();
  auto* then_block = llvm::BasicBlock::Create(owner_.context_, "if.then", function);
  auto* else_block = llvm::BasicBlock::Create(owner_.context_, "if.else", function);
  auto* merge_block = llvm::BasicBlock::Create(owner_.context_, "if.merge");
  builder_.CreateCondBr(condition->value, then_block, else_block);

  builder_.SetInsertPoint(then_block);
  LowerStatements(if_statement.true_block->statements);
  const bool then_terminated = IsCurrentBlockTerminated();
  if (!then_terminated) {
    builder_.CreateBr(merge_block);
  }

  builder_.SetInsertPoint(else_block);
  if (if_statement.else_tail != nullptr) {
    LowerElseTail(*if_statement.else_tail);
  }
  const bool else_terminated = IsCurrentBlockTerminated();
  if (!else_terminated) {
    builder_.CreateBr(merge_block);
  }

  if (then_terminated && else_terminated) {
    return;
  }

  function->insert(function->end(), merge_block);
  builder_.SetInsertPoint(merge_block);
}

void FunctionLoweringContext::LowerStatement(const Statement& statement) {
  if (IsCurrentBlockTerminated()) {
    return;
  }

  std::visit(
      Utils::Overload{
          [this](const DeclarationStatement& declaration) {
            LowerDeclaration(declaration);
          },
          [](const FunctionDeclarationStatement&) {
          },
          [](const ClassDeclarationStatement&) {
          },
          [this](const AssignmentStatement& assignment) {
            LowerAssignment(assignment);
          },
          [this](const PrintStatement& print_statement) {
            LowerPrint(print_statement);
          },
          [this](const DeleteStatement& delete_statement) {
            LowerDelete(delete_statement);
          },
          [this](const IfStatement& if_statement) {
            LowerIf(if_statement);
          },
          [this](const ReturnStatement& return_statement) {
            LowerReturn(return_statement);
          },
          [this](const Expression& expression) {
            LowerExpression(expression);
          },
          [this](const Block& block) {
            LowerStatements(block.statements);
          }},
      statement.value);
}

void FunctionLoweringContext::LowerStatements(const List<Statement>& statements) {
  for (const auto& statement : statements) {
    assert(statement != nullptr);
    LowerStatement(*statement);
  }
}

void FunctionLoweringContext::EmitDefaultReturnIfNeeded() {
  if (IsCurrentBlockTerminated()) {
    return;
  }

  if (!declared_return_type_.has_value()) {
    builder_.CreateRetVoid();
    return;
  }

  builder_.CreateRet(owner_.llvm_utils_.BuildDefaultConstant(*declared_return_type_));
}

void FunctionLoweringContext::EmitClassDefaultInitializers(
    const ClassDeclarationStatement& class_declaration,
    llvm::Value* object_pointer) {
  const ClassDeclarationStatement* base_class =
      owner_.state_.class_relations.GetBaseClass(class_declaration);
  if (base_class != nullptr) {
    llvm::Value* base_ptr = builder_.CreateStructGEP(
        owner_.llvm_class_data_by_decl_.at(&class_declaration).type,
        object_pointer,
        0);
    EmitClassDefaultInitializers(*base_class, base_ptr);
  }

  llvm::Value* previous_this_value = current_this_value_;
  current_this_value_ = object_pointer;

  for (const DeclarationStatement& field : class_declaration.fields) {
    if (field.initializer == nullptr) {
      continue;
    }

    llvm::Value* field_ptr = BuildFieldPointer(
        class_declaration,
        field,
        object_pointer);
    StoreIntoPointer(field_ptr, field.type, *field.initializer);
  }

  current_this_value_ = previous_this_value;
}

}  // namespace

std::string LowerToLLVMIR(
    const Program& program,
    const UseResolver& use_resolver,
    const TypeDefiner& type_definer) {
  PreparedStateBuilder preparation_builder(program, type_definer);
  PreparedState state = preparation_builder.Build();

  LLVMModuleBuilder lowering_builder(
      use_resolver,
      type_definer,
      std::move(state));
  return lowering_builder.BuildIR();
}

}  // namespace Parsing
