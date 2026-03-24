#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "Parsing/Types.hpp"

namespace Parsing {

#define ADD_OPERATOR_EQUAL(type) \
  friend bool operator==(const type& left, const type& right) = default;

class ASTNode {
 public:
  friend bool operator==(const ASTNode& a, const ASTNode& b) = default;
};

template <typename T>
using List = std::vector<std::unique_ptr<T>>;

struct IdentifierExpression : ASTNode {
  IdentifierExpression() = default;

  explicit IdentifierExpression(std::string name_in)
      : name(std::move(name_in)) {}

  std::string name;

  ADD_OPERATOR_EQUAL(IdentifierExpression)
};

using LiteralValue = std::variant<int, bool>;

struct LiteralExpression : ASTNode {
  LiteralExpression() = default;

  explicit LiteralExpression(LiteralValue value_in)
      : value(std::move(value_in)) {}

  LiteralValue value;

  ADD_OPERATOR_EQUAL(LiteralExpression)
};

struct Expression;

struct UnaryPlusExpression;
struct UnaryMinusExpression;
struct UnaryNotExpression;
struct AddExpression;
struct SubtractExpression;
struct MultiplyExpression;
struct DivideExpression;
struct ModuloExpression;
struct LogicalAndExpression;
struct LogicalOrExpression;
struct EqualExpression;
struct NotEqualExpression;
struct LessExpression;
struct GreaterExpression;
struct LessEqualExpression;
struct GreaterEqualExpression;
struct FunctionCall;

struct UnaryOperationBase : ASTNode {
  UnaryOperationBase() = default;

  explicit UnaryOperationBase(std::unique_ptr<Expression> operand_in)
      : operand(std::move(operand_in)) {}

  std::unique_ptr<Expression> operand;

  ADD_OPERATOR_EQUAL(UnaryOperationBase)
};

struct BinaryOperationBase : ASTNode {
  BinaryOperationBase() = default;

  BinaryOperationBase(
      std::unique_ptr<Expression> left_in,
      std::unique_ptr<Expression> right_in)
      : left(std::move(left_in)),
        right(std::move(right_in)) {}

  std::unique_ptr<Expression> left;
  std::unique_ptr<Expression> right;

  ADD_OPERATOR_EQUAL(BinaryOperationBase)
};

template <typename T>
concept BinaryExpressionNode =
    std::derived_from<std::remove_cvref_t<T>, BinaryOperationBase>;

template <typename T>
concept UnaryExpressionNode =
    std::derived_from<std::remove_cvref_t<T>, UnaryOperationBase>;

struct UnaryPlusExpression : UnaryOperationBase {
  using UnaryOperationBase::UnaryOperationBase;

  ADD_OPERATOR_EQUAL(UnaryPlusExpression)
};

struct UnaryMinusExpression : UnaryOperationBase {
  using UnaryOperationBase::UnaryOperationBase;

  ADD_OPERATOR_EQUAL(UnaryMinusExpression)
};

struct UnaryNotExpression : UnaryOperationBase {
  using UnaryOperationBase::UnaryOperationBase;

  ADD_OPERATOR_EQUAL(UnaryNotExpression)
};

struct AddExpression : BinaryOperationBase {
  using BinaryOperationBase::BinaryOperationBase;

  ADD_OPERATOR_EQUAL(AddExpression)
};

struct SubtractExpression : BinaryOperationBase {
  using BinaryOperationBase::BinaryOperationBase;

  ADD_OPERATOR_EQUAL(SubtractExpression)
};

struct MultiplyExpression : BinaryOperationBase {
  using BinaryOperationBase::BinaryOperationBase;

  ADD_OPERATOR_EQUAL(MultiplyExpression)
};

struct DivideExpression : BinaryOperationBase {
  using BinaryOperationBase::BinaryOperationBase;

  ADD_OPERATOR_EQUAL(DivideExpression)
};

struct ModuloExpression : BinaryOperationBase {
  using BinaryOperationBase::BinaryOperationBase;

  ADD_OPERATOR_EQUAL(ModuloExpression)
};

struct LogicalAndExpression : BinaryOperationBase {
  using BinaryOperationBase::BinaryOperationBase;

  ADD_OPERATOR_EQUAL(LogicalAndExpression)
};

struct LogicalOrExpression : BinaryOperationBase {
  using BinaryOperationBase::BinaryOperationBase;

  ADD_OPERATOR_EQUAL(LogicalOrExpression)
};

struct EqualExpression : BinaryOperationBase {
  using BinaryOperationBase::BinaryOperationBase;

  ADD_OPERATOR_EQUAL(EqualExpression)
};

struct NotEqualExpression : BinaryOperationBase {
  using BinaryOperationBase::BinaryOperationBase;

  ADD_OPERATOR_EQUAL(NotEqualExpression)
};

struct LessExpression : BinaryOperationBase {
  using BinaryOperationBase::BinaryOperationBase;

  ADD_OPERATOR_EQUAL(LessExpression)
};

struct GreaterExpression : BinaryOperationBase {
  using BinaryOperationBase::BinaryOperationBase;

  ADD_OPERATOR_EQUAL(GreaterExpression)
};

struct LessEqualExpression : BinaryOperationBase {
  using BinaryOperationBase::BinaryOperationBase;

  ADD_OPERATOR_EQUAL(LessEqualExpression)
};

struct GreaterEqualExpression : BinaryOperationBase {
  using BinaryOperationBase::BinaryOperationBase;

  ADD_OPERATOR_EQUAL(GreaterEqualExpression)
};

struct NamedCallArgument : ASTNode {
  NamedCallArgument() = default;

  NamedCallArgument(
      std::string name_in,
      std::unique_ptr<Expression> value_in)
      : name(std::move(name_in)),
        value(std::move(value_in)) {}

  std::string name;
  std::unique_ptr<Expression> value;

  ADD_OPERATOR_EQUAL(NamedCallArgument)
};

struct PositionalCallArgument : ASTNode {
  PositionalCallArgument() = default;

  explicit PositionalCallArgument(std::unique_ptr<Expression> value_in)
      : value(std::move(value_in)) {}

  std::unique_ptr<Expression> value;

  ADD_OPERATOR_EQUAL(PositionalCallArgument)
};

using CallArgument = std::variant<NamedCallArgument, PositionalCallArgument>;

struct FunctionCall : ASTNode {
  FunctionCall() = default;

  FunctionCall(std::string function_name_in, List<CallArgument> arguments_in)
      : function_name(std::move(function_name_in)),
        arguments(std::move(arguments_in)) {}

  std::string function_name;
  List<CallArgument> arguments;

  ADD_OPERATOR_EQUAL(FunctionCall)
};

using ExpressionVariant = std::variant<
    IdentifierExpression,
    LiteralExpression,
    UnaryPlusExpression,
    UnaryMinusExpression,
    UnaryNotExpression,
    AddExpression,
    SubtractExpression,
    MultiplyExpression,
    DivideExpression,
    ModuloExpression,
    LogicalAndExpression,
    LogicalOrExpression,
    EqualExpression,
    NotEqualExpression,
    LessExpression,
    GreaterExpression,
    LessEqualExpression,
    GreaterEqualExpression,
    FunctionCall>;

struct Expression : ASTNode {
  Expression() = default;

  explicit Expression(ExpressionVariant value_in)
      : value(std::move(value_in)) {}

  ExpressionVariant value;

  ADD_OPERATOR_EQUAL(Expression)
};

struct AssignmentStatement : ASTNode {
  AssignmentStatement() = default;

  AssignmentStatement(
      std::string variable_name_in,
      std::unique_ptr<Expression> expr_in)
      : variable_name(std::move(variable_name_in)),
        expr(std::move(expr_in)) {}

  std::string variable_name;
  std::unique_ptr<Expression> expr;

  ADD_OPERATOR_EQUAL(AssignmentStatement)
};

struct PrintStatement : ASTNode {
  PrintStatement() = default;

  explicit PrintStatement(std::unique_ptr<Expression> expr_in)
      : expr(std::move(expr_in)) {}

  std::unique_ptr<Expression> expr;

  ADD_OPERATOR_EQUAL(PrintStatement)
};

struct ReturnStatement : ASTNode {
  ReturnStatement() = default;

  explicit ReturnStatement(std::unique_ptr<Expression> expr_in)
      : expr(std::move(expr_in)) {}

  std::unique_ptr<Expression> expr;

  ADD_OPERATOR_EQUAL(ReturnStatement)
};

struct FunctionParameter : ASTNode {
  FunctionParameter() = default;

  FunctionParameter(std::string name_in, Type type_in)
      : name(std::move(name_in)),
        type(std::move(type_in)) {}

  std::string name;
  Type type;

  ADD_OPERATOR_EQUAL(FunctionParameter)
};

struct Block;
struct Statement;
struct IfStatement;
struct ElseTail;

struct FunctionDeclarationStatement : ASTNode {
  FunctionDeclarationStatement() = default;

  FunctionDeclarationStatement(
      std::string function_name_in,
      std::vector<FunctionParameter> parameters_in,
      std::optional<Type> return_type_in,
      std::unique_ptr<Block> body_in)
      : function_name(std::move(function_name_in)),
        parameters(std::move(parameters_in)),
        return_type(std::move(return_type_in)),
        body(std::move(body_in)) {}

  std::string function_name;
  std::vector<FunctionParameter> parameters;
  std::optional<Type> return_type;
  std::unique_ptr<Block> body;

  ADD_OPERATOR_EQUAL(FunctionDeclarationStatement)
};

struct DeclarationStatement : ASTNode {
  DeclarationStatement() = default;

  DeclarationStatement(
      std::string variable_name_in,
      Type type_in,
      bool is_mutable_in,
      std::unique_ptr<Expression> initializer_in)
      : variable_name(std::move(variable_name_in)),
        type(std::move(type_in)),
        is_mutable(is_mutable_in),
        initializer(std::move(initializer_in)) {}

  std::string variable_name;
  Type type;
  bool is_mutable;
  std::unique_ptr<Expression> initializer;

  ADD_OPERATOR_EQUAL(DeclarationStatement)
};

struct IfStatement : ASTNode {
  IfStatement() = default;

  IfStatement(
      std::unique_ptr<Expression> condition_in,
      std::unique_ptr<Block> true_block_in,
      std::unique_ptr<ElseTail> else_tail_in)
      : condition(std::move(condition_in)),
        true_block(std::move(true_block_in)),
        else_tail(std::move(else_tail_in)) {}

  std::unique_ptr<Expression> condition;
  std::unique_ptr<Block> true_block;
  std::unique_ptr<ElseTail> else_tail;

  ADD_OPERATOR_EQUAL(IfStatement)
};

struct ElseTail : ASTNode {
  ElseTail() = default;

  ElseTail(
      std::unique_ptr<IfStatement> else_if_in,
      std::unique_ptr<Block> else_block_in)
      : else_if(std::move(else_if_in)),
        else_block(std::move(else_block_in)) {}

  std::unique_ptr<IfStatement> else_if;
  std::unique_ptr<Block> else_block;

  ADD_OPERATOR_EQUAL(ElseTail)
};

struct Block : ASTNode {
  Block() = default;

  explicit Block(List<Statement> statements_in)
      : statements(std::move(statements_in)) {}

  List<Statement> statements;

  ADD_OPERATOR_EQUAL(Block)
};

using StatementVariant = std::variant<
    DeclarationStatement,
    FunctionDeclarationStatement,
    AssignmentStatement,
    PrintStatement,
    IfStatement,
    ReturnStatement,
    Expression,
    Block>;

struct Statement : ASTNode {
  Statement() = default;

  explicit Statement(StatementVariant value_in)
      : value(std::move(value_in)) {}

  StatementVariant value;

  ADD_OPERATOR_EQUAL(Statement)
};

struct Program : ASTNode {
  Program() = default;

  explicit Program(List<Statement> top_statements_in)
      : top_statements(std::move(top_statements_in)) {}

  List<Statement> top_statements;

  ADD_OPERATOR_EQUAL(Program)
};

#undef ADD_OPERATOR_EQUAL

}  // namespace Parsing
