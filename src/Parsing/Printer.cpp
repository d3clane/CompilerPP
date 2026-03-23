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
std::string PrintFunctionCall(const FunctionCall& function_call);
std::string PrintBlock(const Block& block);
std::string PrintIfStatement(const IfStatement& if_statement);
std::string PrintStatementNode(const Statement& statement);
std::string PrintType(const Type& type);

constexpr int kLogicalOrPrecedence = 1;
constexpr int kLogicalAndPrecedence = 2;
constexpr int kEqualityPrecedence = 3;
constexpr int kRelationalPrecedence = 4;
constexpr int kAdditivePrecedence = 5;
constexpr int kMultiplicativePrecedence = 6;
constexpr int kUnaryPrecedence = 7;
constexpr int kAtomPrecedence = 8;

std::string PrintType(const Type& type) {
  return std::visit(
      Utils::Overload{
          [](const IntType&) -> std::string { return "int"; },
          [](const BoolType&) -> std::string { return "bool"; }},
      type);
}

int GetPrecedence(const Expression& expression) {
  return std::visit(
      Utils::Overload{
          [](const IdentifierExpression&) -> int { return kAtomPrecedence; },
          [](const LiteralExpression&) -> int { return kAtomPrecedence; },
          [](const FunctionCall&) -> int { return kAtomPrecedence; },
          [](const UnaryPlusExpression&) -> int { return kUnaryPrecedence; },
          [](const UnaryMinusExpression&) -> int { return kUnaryPrecedence; },
          [](const UnaryNotExpression&) -> int { return kUnaryPrecedence; },
          [](const AddExpression&) -> int { return kAdditivePrecedence; },
          [](const SubtractExpression&) -> int { return kAdditivePrecedence; },
          [](const MultiplyExpression&) -> int { return kMultiplicativePrecedence; },
          [](const DivideExpression&) -> int { return kMultiplicativePrecedence; },
          [](const ModuloExpression&) -> int { return kMultiplicativePrecedence; },
          [](const LogicalAndExpression&) -> int { return kLogicalAndPrecedence; },
          [](const LogicalOrExpression&) -> int { return kLogicalOrPrecedence; },
          [](const EqualExpression&) -> int { return kEqualityPrecedence; },
          [](const NotEqualExpression&) -> int { return kEqualityPrecedence; },
          [](const LessExpression&) -> int { return kRelationalPrecedence; },
          [](const GreaterExpression&) -> int { return kRelationalPrecedence; },
          [](const LessEqualExpression&) -> int { return kRelationalPrecedence; },
          [](const GreaterEqualExpression&) -> int { return kRelationalPrecedence; }},
      expression.value);
}

bool IsBinaryExpression(const Expression& expression) {
  return std::visit(
      [](const auto& node) -> bool {
        return BinaryExpressionNode<decltype(node)>;
      },
      expression.value);
}

bool NeedsParentheses(
    const Expression& expression,
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

std::string PrintFunctionCall(const FunctionCall& function_call) {
  std::string result = function_call.function_name + "(";

  for (size_t i = 0; i < function_call.arguments.size(); ++i) {
    assert(function_call.arguments[i] != nullptr);

    result += std::visit(
        Utils::Overload{
            [](const NamedCallArgument& argument) -> std::string {
              assert(argument.value != nullptr);
              return argument.name + ": " + PrintExpression(*argument.value);
            },
            [](const PositionalCallArgument& argument) -> std::string {
              assert(argument.value != nullptr);
              return PrintExpression(*argument.value);
            }},
        *function_call.arguments[i]);

    if (i + 1 < function_call.arguments.size()) {
      result += ", ";
    }
  }

  result += ")";
  return result;
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
          [](const LiteralExpression& literal) -> std::string {
            return std::visit(
                Utils::Overload{
                    [](int value) -> std::string { return std::to_string(value); },
                    [](bool value) -> std::string { return value ? "true" : "false"; }},
                literal.value);
          },
          [](const FunctionCall& function_call) -> std::string {
            return PrintFunctionCall(function_call);
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
          [](const UnaryNotExpression& unary_expression) -> std::string {
            assert(unary_expression.operand != nullptr);
            return "!" + PrintExpressionWithContext(
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
          },
          [](const LogicalAndExpression& and_expression) -> std::string {
            assert(and_expression.left != nullptr);
            assert(and_expression.right != nullptr);
            return PrintBinaryExpression(
                *and_expression.left,
                "&&",
                *and_expression.right,
                kLogicalAndPrecedence);
          },
          [](const LogicalOrExpression& or_expression) -> std::string {
            assert(or_expression.left != nullptr);
            assert(or_expression.right != nullptr);
            return PrintBinaryExpression(
                *or_expression.left,
                "||",
                *or_expression.right,
                kLogicalOrPrecedence);
          },
          [](const EqualExpression& equal_expression) -> std::string {
            assert(equal_expression.left != nullptr);
            assert(equal_expression.right != nullptr);
            return PrintBinaryExpression(
                *equal_expression.left,
                "==",
                *equal_expression.right,
                kEqualityPrecedence);
          },
          [](const NotEqualExpression& not_equal_expression) -> std::string {
            assert(not_equal_expression.left != nullptr);
            assert(not_equal_expression.right != nullptr);
            return PrintBinaryExpression(
                *not_equal_expression.left,
                "!=",
                *not_equal_expression.right,
                kEqualityPrecedence);
          },
          [](const LessExpression& less_expression) -> std::string {
            assert(less_expression.left != nullptr);
            assert(less_expression.right != nullptr);
            return PrintBinaryExpression(
                *less_expression.left,
                "<",
                *less_expression.right,
                kRelationalPrecedence);
          },
          [](const GreaterExpression& greater_expression) -> std::string {
            assert(greater_expression.left != nullptr);
            assert(greater_expression.right != nullptr);
            return PrintBinaryExpression(
                *greater_expression.left,
                ">",
                *greater_expression.right,
                kRelationalPrecedence);
          },
          [](const LessEqualExpression& less_equal_expression) -> std::string {
            assert(less_equal_expression.left != nullptr);
            assert(less_equal_expression.right != nullptr);
            return PrintBinaryExpression(
                *less_equal_expression.left,
                "<=",
                *less_equal_expression.right,
                kRelationalPrecedence);
          },
          [](const GreaterEqualExpression& greater_equal_expression) -> std::string {
            assert(greater_equal_expression.left != nullptr);
            assert(greater_equal_expression.right != nullptr);
            return PrintBinaryExpression(
                *greater_equal_expression.left,
                ">=",
                *greater_equal_expression.right,
                kRelationalPrecedence);
          }},
      expression.value);

  if (NeedsParentheses(expression, parent_precedence, is_right_child)) {
    return "(" + printed + ")";
  }

  return printed;
}

std::string PrintExpression(const Expression& expression) {
  return PrintExpressionWithContext(expression, 0, false);
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

  std::string result = "if " + PrintExpression(*if_statement.condition) + " " +
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

std::string PrintDeclarationStatement(const DeclarationStatement& declaration) {
  std::string result = "var ";
  if (declaration.is_mutable) {
    result += "mutable ";
  }

  result += declaration.variable_name + " " + PrintType(declaration.type);

  if (declaration.initializer != nullptr) {
    result += " = " + PrintExpression(*declaration.initializer);
  }

  result += ";";
  return result;
}

std::string PrintFunctionDeclarationStatement(
    const FunctionDeclarationStatement& function_declaration) {
  assert(function_declaration.body != nullptr);

  std::string result = "func " + function_declaration.function_name + "(";
  for (size_t i = 0; i < function_declaration.parameters.size(); ++i) {
    result += function_declaration.parameters[i].name +
              " " +
              PrintType(function_declaration.parameters[i].type);
    if (i + 1 < function_declaration.parameters.size()) {
      result += ", ";
    }
  }
  result += ")";

  if (function_declaration.return_type.has_value()) {
    result += " " + PrintType(*function_declaration.return_type);
  }

  result += " " + PrintBlock(*function_declaration.body);
  return result;
}

std::string PrintAssignmentStatement(const AssignmentStatement& assignment) {
  assert(assignment.expr != nullptr);
  return assignment.variable_name +
         " = " +
         PrintExpression(*assignment.expr) +
         ";";
}

std::string PrintPrintStatement(const PrintStatement& print_statement) {
  assert(print_statement.expr != nullptr);
  return "print(" + PrintExpression(*print_statement.expr) + ");";
}

std::string PrintReturnStatement(const ReturnStatement& return_statement) {
  if (return_statement.expr == nullptr) {
    return "return;";
  }
  return "return " + PrintExpression(*return_statement.expr) + ";";
}

std::string PrintStatementNode(const Statement& statement) {
  return std::visit(
      Utils::Overload{
          [](const DeclarationStatement& declaration) -> std::string {
            return PrintDeclarationStatement(declaration);
          },
          [](const FunctionDeclarationStatement& function_declaration) -> std::string {
            return PrintFunctionDeclarationStatement(function_declaration);
          },
          [](const AssignmentStatement& assignment) -> std::string {
            return PrintAssignmentStatement(assignment);
          },
          [](const PrintStatement& print_statement) -> std::string {
            return PrintPrintStatement(print_statement);
          },
          [](const IfStatement& if_statement) -> std::string {
            return PrintIfStatement(if_statement);
          },
          [](const ReturnStatement& return_statement) -> std::string {
            return PrintReturnStatement(return_statement);
          },
          [](const Expression& expression) -> std::string {
            return PrintExpression(expression) + ";";
          },
          [](const Block& block) -> std::string {
            return PrintBlock(block);
          }},
      statement.value);
}

}  // namespace

std::string PrintInfix(const Program& program) {
  std::string result;
  for (size_t i = 0; i < program.top_statements.size(); ++i) {
    assert(program.top_statements[i] != nullptr);
    result += PrintStatementNode(*program.top_statements[i]);
    result += "\n";
  }
  return result;
}

}  // namespace Parsing
