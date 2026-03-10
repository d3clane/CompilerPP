#include "Parsing/Printer.hpp"

#include <cassert>

#include "Utils/Overload.hpp"

namespace Parsing {
namespace {

std::string PrintValue(const ValueVariant& value);
std::string PrintComparisonOperator(const ComparisonOperatorVariant& op);
std::string PrintBoolExpression(const BoolExpression& bool_expression);
std::string PrintStatementNode(const StatementVariant& statement);
std::string PrintTopStatement(const TopStatementVariant& statement);

std::string PrintValue(const ValueVariant& value) {
  return std::visit(
      Utils::Overload{
          [](const IdentifierExpression& identifier) -> std::string {
            return identifier.name;
          },
          [](const NumberExpression& number) -> std::string {
            return std::to_string(number.value);
          }},
      value);
}

std::string PrintComparisonOperator(const ComparisonOperatorVariant& op) {
  return std::visit(
      Utils::Overload{
          [](const EqualComparison&) -> std::string { return "=="; },
          [](const NotEqualComparison&) -> std::string { return "!="; },
          [](const LessComparison&) -> std::string { return "<"; },
          [](const GreaterComparison&) -> std::string { return ">"; },
          [](const LessEqualComparison&) -> std::string { return "<="; },
          [](const GreaterEqualComparison&) -> std::string { return ">="; }},
      op);
}

std::string PrintBoolExpression(const BoolExpression& bool_expression) {
  assert(bool_expression.left != nullptr);
  assert(bool_expression.op != nullptr);
  assert(bool_expression.right != nullptr);

  return PrintValue(*bool_expression.left) + " " +
         PrintComparisonOperator(*bool_expression.op) + " " +
         PrintValue(*bool_expression.right);
}

std::string PrintBlock(const Block& block) {
  if (block.statements.empty()) {
    return "{ }";
  }

  std::string result = "{ ";
  for (int i = 0; i < static_cast<int>(block.statements.size()); ++i) {
    assert(block.statements[i] != nullptr);
    result += PrintStatementNode(*block.statements[i]);
    if (i + 1 < static_cast<int>(block.statements.size())) {
      result += " ";
    }
  }
  result += " }";
  return result;
}

std::string PrintStatementNode(const StatementVariant& statement) {
  return std::visit(
      Utils::Overload{
          [](const AssignmentStatement& assignment) -> std::string {
            assert(assignment.value != nullptr);
            return assignment.variable_name + " = " + PrintValue(*assignment.value) + ";";
          },
          [](const PrintStatement& print_statement) -> std::string {
            assert(print_statement.value != nullptr);
            return "print(" + PrintValue(*print_statement.value) + ");";
          },
          [](const IfStatement& if_statement) -> std::string {
            assert(if_statement.condition != nullptr);
            assert(if_statement.true_block != nullptr);
            assert(if_statement.false_block != nullptr);

            return "if " + PrintBoolExpression(*if_statement.condition) + " " +
                   PrintBlock(*if_statement.true_block) + " else " +
                   PrintBlock(*if_statement.false_block);
          }},
      statement);
}

std::string PrintTopStatement(const TopStatementVariant& statement) {
  return std::visit(
      Utils::Overload{
          [](const DeclarationStatement& declaration) -> std::string {
            return "var " + declaration.variable_name + " " + declaration.type_name + ";";
          },
          [](const AssignmentStatement& assignment) -> std::string {
            assert(assignment.value != nullptr);
            return assignment.variable_name + " = " + PrintValue(*assignment.value) + ";";
          },
          [](const PrintStatement& print_statement) -> std::string {
            assert(print_statement.value != nullptr);
            return "print(" + PrintValue(*print_statement.value) + ");";
          },
          [](const IfStatement& if_statement) -> std::string {
            assert(if_statement.condition != nullptr);
            assert(if_statement.true_block != nullptr);
            assert(if_statement.false_block != nullptr);

            return "if " + PrintBoolExpression(*if_statement.condition) + " " +
                   PrintBlock(*if_statement.true_block) + " else " +
                   PrintBlock(*if_statement.false_block);
          }},
      statement);
}

}  // namespace

std::string PrintInfix(const Program& program) {
  std::string result;
  for (size_t i = 0; i < program.top_statements.size(); ++i) {
    assert(program.top_statements[i] != nullptr);
    result += PrintTopStatement(*program.top_statements[i]);
    if (i + 1 < program.top_statements.size()) {
      result += "\n";
    }
  }
  return result;
}

}  // namespace Parsing
