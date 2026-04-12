#include "Parsing/Types.hpp"

#include <algorithm>

#include "Parsing/Ast.hpp"

namespace Parsing {

namespace {

template <typename T>
T* RemoveAndReturn(
    std::list<std::unique_ptr<T>>& objects,
    const T* object_ptr) {
  for (auto it = objects.begin(); it != objects.end(); ++it) {
    if (it->get() == object_ptr) {
      T* mutable_ptr = it->get();
      objects.erase(it);
      return mutable_ptr;
    }
  }

  return nullptr;
}

}  // namespace

TypeStorage::TypeStorage() {
  bool_type_ = CreateType(Type{BoolType{}});
  int_type_ = CreateType(Type{IntType{}});
}

TypeStorage::~TypeStorage() = default;

const Type* TypeStorage::GetBoolType() const {
  return bool_type_;
}

const Type* TypeStorage::GetIntType() const {
  return int_type_;
}

const Type* TypeStorage::GetOrCreateClassType(const std::string& class_name) {
  const auto existing_it = class_type_by_name_.find(class_name);
  if (existing_it != class_type_by_name_.end()) {
    return existing_it->second;
  }

  const Type* class_type = CreateType(Type{ClassType{}});
  class_type_by_name_[class_name] = class_type;
  return class_type;
}

const Type* TypeStorage::CreateFunctionType(
    const Type* return_type,
    std::vector<const Type*> parameter_types) {
  return CreateType(Type{FuncType{
      .return_type = return_type,
      .parameter_types = std::move(parameter_types)}});
}

const Type* TypeStorage::CreateType(Type type) {
  types_.push_back(std::make_unique<Type>(std::move(type)));
  return types_.back().get();
}

bool TypeStorage::DeleteType(const Type* type) {
  if (type == bool_type_ || type == int_type_) {
    return false;
  }

  const Type* removed_type = RemoveAndReturn(types_, type);
  if (removed_type == nullptr) {
    return false;
  }

  for (auto it = class_type_by_name_.begin(); it != class_type_by_name_.end();) {
    if (it->second == type) {
      it = class_type_by_name_.erase(it);
    } else {
      ++it;
    }
  }

  return true;
}

void TypeStorage::Clear() {
  class_type_by_name_.clear();
  types_.clear();
  bool_type_ = nullptr;
  int_type_ = nullptr;
}

std::optional<const Type*> TypeStorage::FindClassType(
    const std::string& class_name) const {
  const auto class_it = class_type_by_name_.find(class_name);
  if (class_it == class_type_by_name_.end()) {
    return std::nullopt;
  }

  return class_it->second;
}

const FuncType* BuildFunctionType(
    const FunctionDeclarationStatement& function_declaration,
    TypeStorage& storage) {
  if (function_declaration.function_type != nullptr) {
    return AsFuncType(function_declaration.function_type);
  }

  std::vector<const Type*> parameter_types;
  parameter_types.reserve(function_declaration.parameters.size());
  for (const FunctionParameter& parameter : function_declaration.parameters) {
    parameter_types.push_back(parameter.type);
  }

  const Type* function_type = storage.CreateFunctionType(
      nullptr,
      std::move(parameter_types));
  return AsFuncType(function_type);
}

const ClassType* AsClassType(const Type* type) {
  if (type == nullptr) {
    return nullptr;
  }

  return std::get_if<ClassType>(&type->type);
}

const FuncType* AsFuncType(const Type* type) {
  if (type == nullptr) {
    return nullptr;
  }

  return std::get_if<FuncType>(&type->type);
}

bool IsIntType(const Type* type) {
  return type != nullptr && std::holds_alternative<IntType>(type->type);
}

bool IsBoolType(const Type* type) {
  return type != nullptr && std::holds_alternative<BoolType>(type->type);
}

}  // namespace Parsing
