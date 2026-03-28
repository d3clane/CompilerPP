#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace Parsing {

#define ADD_OPERATOR_EQUAL(type) \
  friend bool operator==(const type& left, const type& right) = default;

struct BoolType {
  ADD_OPERATOR_EQUAL(BoolType)
};

struct IntType {
  ADD_OPERATOR_EQUAL(IntType)
};

struct ClassType {
  std::string class_name;

  ADD_OPERATOR_EQUAL(ClassType)
};

struct Type;
struct FuncType;
struct ArrayType;
struct FunctionDeclarationStatement;

struct FuncType {
  std::unique_ptr<Type> return_type;
  std::vector<Type> parameter_types;

  FuncType() = default;
  FuncType(const FuncType& other);
  FuncType& operator=(const FuncType& other);
  FuncType(FuncType&&) noexcept = default;
  FuncType& operator=(FuncType&&) noexcept = default;

  friend bool operator==(const FuncType& left, const FuncType& right);
};

FuncType BuildFunctionType(const FunctionDeclarationStatement& function_declaration);

struct ArrayType {
  std::unique_ptr<Type> element_type;

  ArrayType() = default;
  explicit ArrayType(std::unique_ptr<Type> element_type_in)
      : element_type(std::move(element_type_in)) {}
  ArrayType(const ArrayType& other);
  ArrayType& operator=(const ArrayType& other);
  ArrayType(ArrayType&&) noexcept = default;
  ArrayType& operator=(ArrayType&&) noexcept = default;

  friend bool operator==(const ArrayType& left, const ArrayType& right);
};

using TypeVariant = std::variant<BoolType, IntType, ClassType, ArrayType, FuncType>;

struct Type {
  Type() = default;

  template <typename T>
    requires(!std::same_as<std::remove_cvref_t<T>, Type>)
  explicit Type(T&& type_in)
      : type(std::forward<T>(type_in)) {}

  TypeVariant type;

  ADD_OPERATOR_EQUAL(Type)
};

inline FuncType::FuncType(const FuncType& other)
    : parameter_types(other.parameter_types) {
  if (other.return_type != nullptr) {
    return_type = std::make_unique<Type>(*other.return_type);
  }
}

inline FuncType& FuncType::operator=(const FuncType& other) {
  if (this == &other) {
    return *this;
  }

  parameter_types = other.parameter_types;
  if (other.return_type != nullptr) {
    return_type = std::make_unique<Type>(*other.return_type);
  } else {
    return_type.reset();
  }

  return *this;
}

inline bool operator==(const FuncType& left, const FuncType& right) {
  if ((left.return_type == nullptr) != (right.return_type == nullptr)) {
    return false;
  }

  if (left.return_type != nullptr && right.return_type != nullptr &&
      *left.return_type != *right.return_type) {
    return false;
  }

  return left.parameter_types == right.parameter_types;
}

inline ArrayType::ArrayType(const ArrayType& other) {
  if (other.element_type != nullptr) {
    element_type = std::make_unique<Type>(*other.element_type);
  }
}

inline ArrayType& ArrayType::operator=(const ArrayType& other) {
  if (this == &other) {
    return *this;
  }

  if (other.element_type != nullptr) {
    element_type = std::make_unique<Type>(*other.element_type);
  } else {
    element_type.reset();
  }

  return *this;
}

inline bool operator==(const ArrayType& left, const ArrayType& right) {
  if ((left.element_type == nullptr) != (right.element_type == nullptr)) {
    return false;
  }

  if (left.element_type == nullptr && right.element_type == nullptr) {
    return true;
  }

  return *left.element_type == *right.element_type;
}

#undef ADD_OPERATOR_EQUAL

}  // namespace Parsing
