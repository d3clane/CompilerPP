#include "Parsing/Printer.hpp"

#include <cassert>

#include "Utils/Overload.hpp"

namespace Parsing {
namespace {

std::string PrintExpression(const Expression& expression);
std::string PrintExpressionWithContext(
    const Expression& expression,
    int parent_precedence,
    bool is_right_child);
std::string PrintComparisonOperator(const ComparisonOperatorVariant& op);
std::string PrintBoolExpression(const BoolExpression& bool_expression);
std::string PrintIfStatement(const IfStatement& if_statement);
std::string PrintStatementNode(const StatementVariant& statement);
std::string PrintTopStatement(const TopStatementVariant& statement);

constexpr int kAdditivePrecedence = 1;
constexpr int kMultiplicativePrecedence = 2;
constexpr int kUnaryPrecedence = 3;
constexpr int kAtomPrecedence = 4;

int GetPrecedence(const ExpressionVariant& expression) {
  return std::visit(
      Utils::Overload{
          [](const IdentifierExpression&) -> int { return kAtomPrecedence          ; },
          [](const NumberExpression&)     -> int { return kAtomPrecedence          ; },
          [](const UnaryPlusExpression&)  -> int { return kUnaryPrecedence         ; },
          [](const UnaryMinusExpression&) -> int { return kUnaryPrecedence         ; },
          [](const AddExpression&)        -> int { return kAdditivePrecedence      ; },
          [](const SubtractExpression&)   -> int { return kAdditivePrecedence      ; },
          [](const MultiplyExpression&)   -> int { return kMultiplicativePrecedence; },
          [](const DivideExpression&)     -> int { return kMultiplicativePrecedence; },
          [](const ModuloExpression&)     -> int { return kMultiplicativePrecedence; }},
      expression);
}

bool IsBinaryExpression(const ExpressionVariant& expression) {
  return std::visit(
      Utils::Overload{
          [](const IdentifierExpression&) -> bool { return false; },
          [](const NumberExpression&)     -> bool { return false; },
          [](const UnaryPlusExpression&)  -> bool { return false; },
          [](const UnaryMinusExpression&) -> bool { return false; },
          [](const AddExpression&)        -> bool { return true ; },
          [](const SubtractExpression&)   -> bool { return true ; },
          [](const MultiplyExpression&)   -> bool { return true ; },
          [](const DivideExpression&)     -> bool { return true ; },
          [](const ModuloExpression&)     -> bool { return true ; }},
      expression);
}

bool NeedsParentheses(
    const ExpressionVariant& expression,
    int parent_precedence,
    bool is_right_child) {
  const int current_precedence = GetPrecedence(expression);

  if (current_precedence < parent_precedence) {
    return true;
  }

  if (is_right_child &&
      current_precedence == parent_precedence &&
      IsBinaryExpression(expression)) {
    return true;
  }

  return false;
}

std::string PrintBinaryExpression(
    const Expression& left,
    const std::string& op,
    const Expression& right,
    int precedence) {
  return PrintExpressionWithContext(left, precedence, false) + " " +
         op + " " +
         PrintExpressionWithContext(right, precedence, true);
}

std::string PrintExpressionWithContext(
    const Expression& expression,
    int parent_precedence,
    bool is_right_child) {
  const std::string printed = std::visit(
      Utils::Overload{
          [](const IdentifierExpression& identifier) -> std::string {
            return identifier.name;
          },
          [](const NumberExpression& number) -> std::string {
            return std::to_string(number.value);
          },
          [](const UnaryPlusExpression& unary_expression) -> std::string {
            assert(unary_expression.operand != nullptr);
            return "+" + PrintExpressionWithContext(
                             *unary_expression.operand,
                             kUnaryPrecedence,
                             true);
          },
          [](const UnaryMinusExpression& unary_expression) -> std::string {
            assert(unary_expression.operand != nullptr);
            return "-" + PrintExpressionWithContext(
                             *unary_expression.operand,
                             kUnaryPrecedence,
                             true);
          },
          [](const AddExpression& add_expression) -> std::string {
            assert(add_expression.left != nullptr);
            assert(add_expression.right != nullptr);
            return PrintBinaryExpression(
                *add_expression.left,
                "+",
                *add_expression.right,
                kAdditivePrecedence);
          },
          [](const SubtractExpression& subtract_expression) -> std::string {
            assert(subtract_expression.left != nullptr);
            assert(subtract_expression.right != nullptr);
            return PrintBinaryExpression(
                *subtract_expression.left,
                "-",
                *subtract_expression.right,
                kAdditivePrecedence);
          },
          [](const MultiplyExpression& multiply_expression) -> std::string {
            assert(multiply_expression.left != nullptr);
            assert(multiply_expression.right != nullptr);
            return PrintBinaryExpression(
                *multiply_expression.left,
                "*",
                *multiply_expression.right,
                kMultiplicativePrecedence);
          },
          [](const DivideExpression& divide_expression) -> std::string {
            assert(divide_expression.left != nullptr);
            assert(divide_expression.right != nullptr);
            return PrintBinaryExpression(
                *divide_expression.left,
                "/",
                *divide_expression.right,
                kMultiplicativePrecedence);
          },
          [](const ModuloExpression& modulo_expression) -> std::string {
            assert(modulo_expression.left != nullptr);
            assert(modulo_expression.right != nullptr);
            return PrintBinaryExpression(
                *modulo_expression.left,
                "%",
                *modulo_expression.right,
                kMultiplicativePrecedence);
          }},
      expression.value);

  if (NeedsParentheses(expression.value, parent_precedence, is_right_child)) {
    return "(" + printed + ")";
  }

  return printed;
}

std::string PrintExpression(const Expression& expression) {
  return PrintExpressionWithContext(expression, 0, false);
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

  return PrintExpression(*bool_expression.left) + " " +
         PrintComparisonOperator(*bool_expression.op) + " " +
         PrintExpression(*bool_expression.right);
}

std::string PrintBlock(const Block& block) {
  if (block.statements.empty()) {
    return "{ }";
  }

  std::string result = "{ ";
  for (size_t i = 0; i < block.statements.size(); ++i) {
    assert(block.statements[i] != nullptr);
    result += PrintStatementNode(*block.statements[i]);
    if (i + 1 < block.statements.size()) {
      result += " ";
    }
  }
  result += " }";
  return result;
}

std::string PrintIfStatement(const IfStatement& if_statement) {
  assert(if_statement.condition != nullptr);
  assert(if_statement.true_block != nullptr);
  assert(if_statement.else_tail != nullptr);
  assert(!(if_statement.else_tail->else_if != nullptr &&
           if_statement.else_tail->else_block != nullptr));

  std::string result = "if " + PrintBoolExpression(*if_statement.condition) + " " +
                       PrintBlock(*if_statement.true_block);

  if (if_statement.else_tail->else_if != nullptr) {
    result += " else " + PrintIfStatement(*if_statement.else_tail->else_if);
    return result;
  }

  if (if_statement.else_tail->else_block != nullptr) {
    result += " else " + PrintBlock(*if_statement.else_tail->else_block);
  }

  return result;
}

std::string PrintStatementNode(const StatementVariant& statement) {
  return std::visit(
      Utils::Overload{
          [](const AssignmentStatement& assignment) -> std::string {
            assert(assignment.expr != nullptr);
            return assignment.variable_name + " = " + PrintExpression(*assignment.expr) + ";";
          },
          [](const PrintStatement& print_statement) -> std::string {
            assert(print_statement.expr != nullptr);
            return "print(" + PrintExpression(*print_statement.expr) + ");";
          },
          [](const IfStatement& if_statement) -> std::string {
            return PrintIfStatement(if_statement);
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
            assert(assignment.expr != nullptr);
            return assignment.variable_name + " = " + PrintExpression(*assignment.expr) + ";";
          },
          [](const PrintStatement& print_statement) -> std::string {
            assert(print_statement.expr != nullptr);
            return "print(" + PrintExpression(*print_statement.expr) + ");";
          },
          [](const IfStatement& if_statement) -> std::string {
            return PrintIfStatement(if_statement);
          }},
      statement);
}

}  // namespace

std::string PrintInfix(const Program& program) {
  std::string result;
  for (size_t i = 0; i < program.top_statements.size(); ++i) {
    assert(program.top_statements[i] != nullptr);
    result += PrintTopStatement(*program.top_statements[i]) + "\n";
  }
  return result;
}

}  // namespace Parsing
