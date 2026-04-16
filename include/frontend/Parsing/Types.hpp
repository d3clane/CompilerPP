#pragma once

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace Front {

struct IdentifierExpression;
struct FunctionDeclarationStatement;
struct ClassDeclarationStatement;
struct Type;
struct FuncType;
struct ClassType;

#define ADD_OPERATOR_EQUAL(type) \
  friend bool operator==(const type& left, const type& right) = default;

struct BoolType {
  ADD_OPERATOR_EQUAL(BoolType)
};

struct IntType {
  ADD_OPERATOR_EQUAL(IntType)
};

struct ClassFieldType {
  const Type* type = nullptr;
  const IdentifierExpression* name = nullptr;

  ADD_OPERATOR_EQUAL(ClassFieldType)
};

struct ClassMethodType {
  const FuncType* type = nullptr;
  const IdentifierExpression* name = nullptr;

  ADD_OPERATOR_EQUAL(ClassMethodType)
};

struct ClassType {
  const ClassDeclarationStatement* class_decl = nullptr;
  const ClassType* base_class = nullptr;
  std::vector<ClassFieldType> fields;
  std::vector<ClassMethodType> methods;

  ADD_OPERATOR_EQUAL(ClassType)
};

struct FuncType {
  const Type* return_type = nullptr;
  std::vector<const Type*> parameter_types;

  ADD_OPERATOR_EQUAL(FuncType)
};

using TypeVariant = std::variant<BoolType, IntType, ClassType, FuncType>;

struct Type {
  Type() = default;

  template <typename T>
    requires(!std::same_as<std::remove_cvref_t<T>, Type>)
  explicit Type(T&& type_in)
      : type(std::forward<T>(type_in)) {}

  TypeVariant type;

  ADD_OPERATOR_EQUAL(Type)
};

class TypeStorage {
 public:
  TypeStorage();
  ~TypeStorage();

  TypeStorage(const TypeStorage&) = delete;
  TypeStorage& operator=(const TypeStorage&) = delete;
  TypeStorage(TypeStorage&&) noexcept = default;
  TypeStorage& operator=(TypeStorage&&) noexcept = default;

  const Type* GetBoolType() const;
  const Type* GetIntType() const;

  const Type* GetOrCreateClassType(const std::string& class_name);
  const Type* CreateFunctionType(
      const Type* return_type,
      std::vector<const Type*> parameter_types);

  const Type* CreateType(Type type);
  bool DeleteType(const Type* type);

  void Clear();

  std::optional<const Type*> FindClassType(const std::string& class_name) const;

 private:
  const Type* bool_type_ = nullptr;
  const Type* int_type_ = nullptr;
  std::list<std::unique_ptr<Type>> types_;
  std::map<std::string, const Type*> class_type_by_name_;
};

const FuncType* BuildFunctionType(
    const FunctionDeclarationStatement& function_declaration,
    TypeStorage& storage);

const ClassType* AsClassType(const Type* type);
const FuncType* AsFuncType(const Type* type);
bool IsIntType(const Type* type);
bool IsBoolType(const Type* type);

#undef ADD_OPERATOR_EQUAL

}  // namespace Front
