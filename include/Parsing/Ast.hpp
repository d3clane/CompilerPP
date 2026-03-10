#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace Parsing {

#define ADD_NODE(name) \
  struct name { \
    friend bool operator==(const name& left, const name& right) = default; \
  };

#define ADD_OPERATOR_EQUAL(type) \
  friend bool operator==(const type& left, const type& right) = default;

struct IdentifierExpression {
  std::string name;

  ADD_OPERATOR_EQUAL(IdentifierExpression)
};

struct NumberExpression {
  int value;

  ADD_OPERATOR_EQUAL(NumberExpression)
};

using ValueVariant = std::variant<IdentifierExpression, NumberExpression>;

ADD_NODE(EqualComparison)
ADD_NODE(NotEqualComparison)
ADD_NODE(LessComparison)
ADD_NODE(GreaterComparison)
ADD_NODE(LessEqualComparison)
ADD_NODE(GreaterEqualComparison)

using ComparisonOperatorVariant = std::variant<
    EqualComparison,
    NotEqualComparison,
    LessComparison,
    GreaterComparison,
    LessEqualComparison,
    GreaterEqualComparison>;

struct BoolExpression {
  std::unique_ptr<ValueVariant> left;
  std::unique_ptr<ComparisonOperatorVariant> op;
  std::unique_ptr<ValueVariant> right;

  ADD_OPERATOR_EQUAL(BoolExpression)
};

struct Block;

struct AssignmentStatement {
  std::string variable_name;
  std::unique_ptr<ValueVariant> value;

  ADD_OPERATOR_EQUAL(AssignmentStatement)
};

struct PrintStatement {
  std::unique_ptr<ValueVariant> value;

  ADD_OPERATOR_EQUAL(PrintStatement)
};

struct IfStatement {
  std::unique_ptr<BoolExpression> condition;
  std::unique_ptr<Block> true_block;
  std::unique_ptr<Block> false_block;

  ADD_OPERATOR_EQUAL(IfStatement)
};

using StatementVariant = std::variant<AssignmentStatement, PrintStatement, IfStatement>;

struct Block {
  std::vector<std::unique_ptr<StatementVariant>> statements;

  ADD_OPERATOR_EQUAL(Block)
};

struct DeclarationStatement {
  std::string variable_name;
  std::string type_name;

  ADD_OPERATOR_EQUAL(DeclarationStatement)
};

using TopStatementVariant =
    std::variant<DeclarationStatement, AssignmentStatement, PrintStatement, IfStatement>;

struct Program {
  std::vector<std::unique_ptr<TopStatementVariant>> top_statements;

  ADD_OPERATOR_EQUAL(Program)
};

#undef ADD_NODE
#undef ADD_OPERATOR_EQUAL

}  // namespace Parsing
