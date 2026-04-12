#pragma once

#include <type_traits>

#include "Parsing/Ast.hpp"
#include "SemanticAnalysis/Resolver.hpp"

namespace Parsing {

class DebugCtx;

template <typename... SupportedOperationTypes>
struct SupportedExprOps {
};

using SupportedBoolExprOps = SupportedExprOps<
    UnaryNotExpression,
    LogicalAndExpression,
    LogicalOrExpression,
    EqualExpression,
    NotEqualExpression>;

using SupportedIntExprOps = SupportedExprOps<
    UnaryPlusExpression,
    UnaryMinusExpression,
    AddExpression,
    SubtractExpression,
    MultiplyExpression,
    DivideExpression,
    ModuloExpression,
    EqualExpression,
    NotEqualExpression,
    LessExpression,
    GreaterExpression,
    LessEqualExpression,
    GreaterEqualExpression>;

template <typename OperationType, typename SupportedExprOpsType>
struct IsExprOpInSupportedList;

template <typename OperationType, typename... SupportedOperationTypes>
struct IsExprOpInSupportedList<
    OperationType,
    SupportedExprOps<SupportedOperationTypes...>>
    : std::bool_constant<
          (std::is_same_v<
               std::remove_cvref_t<OperationType>,
               SupportedOperationTypes> ||
           ...)> {
};

template <typename OperationType, typename ValueType>
struct IsSupportedExprOpTrait : std::false_type {
};

template <typename OperationType>
struct IsSupportedExprOpTrait<OperationType, BoolType>
    : IsExprOpInSupportedList<OperationType, SupportedBoolExprOps> {
};

template <typename OperationType>
struct IsSupportedExprOpTrait<OperationType, IntType>
    : IsExprOpInSupportedList<OperationType, SupportedIntExprOps> {
};

template <typename OperationType, typename ValueType>
constexpr bool IsSupportedExprOp() {
  return IsSupportedExprOpTrait<
      std::remove_cvref_t<OperationType>,
      std::remove_cvref_t<ValueType>>::value;
}

void CheckTypes(
    const Program& program,
    const UseResolver& use_resolver,
    DebugCtx& debug_ctx);

void CheckTypes(
    const Program& program,
    const UseResolver& use_resolver);

}  // namespace Parsing
