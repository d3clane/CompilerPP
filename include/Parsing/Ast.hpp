#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace Parsing {

#define ADD_NODE(name) \
  struct name { \
    friend bool operator==(const name& left, const name& right) = default; \
  };

#define ADD_OPERATOR_EQUAL(type) \
  friend bool operator==(const type& left, const type& right) = default;

template <typename T>
using List = std::vector<std::unique_ptr<T>>;

struct IdentifierExpression {
  std::string name;

  ADD_OPERATOR_EQUAL(IdentifierExpression)
};

using LiteralValue = std::variant<int, bool>;

struct LiteralExpression {
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

struct UnaryOperationBase {
  std::unique_ptr<Expression> operand;

  ADD_OPERATOR_EQUAL(UnaryOperationBase)
};

struct BinaryOperationBase {
  std::unique_ptr<Expression> left;
  std::unique_ptr<Expression> right;

  ADD_OPERATOR_EQUAL(BinaryOperationBase)
};

template <typename T>
concept BinaryExpressionNode =
    std::derived_from<std::remove_cvref_t<T>, BinaryOperationBase>;

struct UnaryPlusExpression : UnaryOperationBase {
  ADD_OPERATOR_EQUAL(UnaryPlusExpression)
};

struct UnaryMinusExpression : UnaryOperationBase {
  ADD_OPERATOR_EQUAL(UnaryMinusExpression)
};

struct UnaryNotExpression : UnaryOperationBase {
  ADD_OPERATOR_EQUAL(UnaryNotExpression)
};

struct AddExpression : BinaryOperationBase {
  ADD_OPERATOR_EQUAL(AddExpression)
};

struct SubtractExpression : BinaryOperationBase {
  ADD_OPERATOR_EQUAL(SubtractExpression)
};

struct MultiplyExpression : BinaryOperationBase {
  ADD_OPERATOR_EQUAL(MultiplyExpression)
};

struct DivideExpression : BinaryOperationBase {
  ADD_OPERATOR_EQUAL(DivideExpression)
};

struct ModuloExpression : BinaryOperationBase {
  ADD_OPERATOR_EQUAL(ModuloExpression)
};

struct LogicalAndExpression : BinaryOperationBase {
  ADD_OPERATOR_EQUAL(LogicalAndExpression)
};

struct LogicalOrExpression : BinaryOperationBase {
  ADD_OPERATOR_EQUAL(LogicalOrExpression)
};

struct EqualExpression : BinaryOperationBase {
  ADD_OPERATOR_EQUAL(EqualExpression)
};

struct NotEqualExpression : BinaryOperationBase {
  ADD_OPERATOR_EQUAL(NotEqualExpression)
};

struct LessExpression : BinaryOperationBase {
  ADD_OPERATOR_EQUAL(LessExpression)
};

struct GreaterExpression : BinaryOperationBase {
  ADD_OPERATOR_EQUAL(GreaterExpression)
};

struct LessEqualExpression : BinaryOperationBase {
  ADD_OPERATOR_EQUAL(LessEqualExpression)
};

struct GreaterEqualExpression : BinaryOperationBase {
  ADD_OPERATOR_EQUAL(GreaterEqualExpression)
};

struct NamedCallArgument {
  std::string name;
  std::unique_ptr<Expression> value;

  ADD_OPERATOR_EQUAL(NamedCallArgument)
};

struct PositionalCallArgument {
  std::unique_ptr<Expression> value;

  ADD_OPERATOR_EQUAL(PositionalCallArgument)
};

using CallArgument = std::variant<NamedCallArgument, PositionalCallArgument>;

struct FunctionCall {
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

struct Expression {
  ExpressionVariant value;

  ADD_OPERATOR_EQUAL(Expression)
};

struct AssignmentStatement {
  std::string variable_name;
  std::unique_ptr<Expression> expr;

  ADD_OPERATOR_EQUAL(AssignmentStatement)
};

struct PrintStatement {
  std::unique_ptr<Expression> expr;

  ADD_OPERATOR_EQUAL(PrintStatement)
};

struct ReturnStatement {
  std::unique_ptr<Expression> expr;

  ADD_OPERATOR_EQUAL(ReturnStatement)
};

ADD_NODE(IntType)
ADD_NODE(BoolType)

using Type = std::variant<IntType, BoolType>;

struct FunctionParameter {
  std::string name;
  Type type;

  ADD_OPERATOR_EQUAL(FunctionParameter)
};

struct Block;
struct Statement;
struct IfStatement;
struct ElseTail;

struct FunctionDeclarationStatement {
  std::string function_name;
  std::vector<FunctionParameter> parameters;
  std::optional<Type> return_type;
  std::unique_ptr<Block> body;

  ADD_OPERATOR_EQUAL(FunctionDeclarationStatement)
};

struct DeclarationStatement {
  std::string variable_name;
  Type type;
  bool is_mutable;
  std::unique_ptr<Expression> initializer;

  ADD_OPERATOR_EQUAL(DeclarationStatement)
};

struct IfStatement {
  std::unique_ptr<Expression> condition;
  std::unique_ptr<Block> true_block;
  std::unique_ptr<ElseTail> else_tail;

  ADD_OPERATOR_EQUAL(IfStatement)
};

struct ElseTail {
  std::unique_ptr<IfStatement> else_if;
  std::unique_ptr<Block> else_block;

  ADD_OPERATOR_EQUAL(ElseTail)
};

struct Block {
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

struct Statement {
  StatementVariant value;

  ADD_OPERATOR_EQUAL(Statement)
};

struct Program {
  List<Statement> top_statements;

  ADD_OPERATOR_EQUAL(Program)
};

#undef ADD_NODE
#undef ADD_OPERATOR_EQUAL

}  // namespace Parsing
