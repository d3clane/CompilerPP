#pragma once

#include <memory>
#include <optional>
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

struct FuncType;

using Type = std::variant<BoolType, IntType, std::shared_ptr<FuncType>>;

struct FuncType {
  std::optional<Type> return_type;
  std::vector<Type> parameter_types;

  ADD_OPERATOR_EQUAL(FuncType)
};

#undef ADD_OPERATOR_EQUAL

}  // namespace Parsing
