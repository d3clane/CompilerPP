#include "Parsing/Printer.hpp"

#include <cassert>
#include <type_traits>

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

template <typename T>
inline constexpr bool kAlwaysFalse = false;

template <UnaryExpressionNode T>
constexpr const char* GetUnaryOperatorToken() {
  if constexpr (std::same_as<T, UnaryPlusExpression>) {
    return "+";
  } else if constexpr (std::same_as<T, UnaryMinusExpression>) {
    return "-";
  } else if constexpr (std::same_as<T, UnaryNotExpression>) {
    return "!";
  } else {
    static_assert(kAlwaysFalse<T>, "Unsupported unary expression node");
  }
}

template <BinaryExpressionNode T>
constexpr const char* GetBinaryOperatorToken() {
  if constexpr (std::same_as<T, AddExpression>) {
    return "+";
  } else if constexpr (std::same_as<T, SubtractExpression>) {
    return "-";
  } else if constexpr (std::same_as<T, MultiplyExpression>) {
    return "*";
  } else if constexpr (std::same_as<T, DivideExpression>) {
    return "/";
  } else if constexpr (std::same_as<T, ModuloExpression>) {
    return "%";
  } else if constexpr (std::same_as<T, LogicalAndExpression>) {
    return "&&";
  } else if constexpr (std::same_as<T, LogicalOrExpression>) {
    return "||";
  } else if constexpr (std::same_as<T, EqualExpression>) {
    return "==";
  } else if constexpr (std::same_as<T, NotEqualExpression>) {
    return "!=";
  } else if constexpr (std::same_as<T, LessExpression>) {
    return "<";
  } else if constexpr (std::same_as<T, GreaterExpression>) {
    return ">";
  } else if constexpr (std::same_as<T, LessEqualExpression>) {
    return "<=";
  } else if constexpr (std::same_as<T, GreaterEqualExpression>) {
    return ">=";
  } else {
    static_assert(kAlwaysFalse<T>, "Unsupported binary expression node");
  }
}

template <BinaryExpressionNode T>
constexpr int GetBinaryPrecedence() {
  if constexpr (
      std::same_as<T, AddExpression> ||
      std::same_as<T, SubtractExpression>) {
    return kAdditivePrecedence;
  } else if constexpr (
      std::same_as<T, MultiplyExpression> ||
      std::same_as<T, DivideExpression> ||
      std::same_as<T, ModuloExpression>) {
    return kMultiplicativePrecedence;
  } else if constexpr (
      std::same_as<T, LogicalAndExpression>) {
    return kLogicalAndPrecedence;
  } else if constexpr (
      std::same_as<T, LogicalOrExpression>) {
    return kLogicalOrPrecedence;
  } else if constexpr (
      std::same_as<T, EqualExpression> ||
      std::same_as<T, NotEqualExpression>) {
    return kEqualityPrecedence;
  } else if constexpr (
      std::same_as<T, LessExpression> ||
      std::same_as<T, GreaterExpression> ||
      std::same_as<T, LessEqualExpression> ||
      std::same_as<T, GreaterEqualExpression>) {
    return kRelationalPrecedence;
  } else {
    static_assert(kAlwaysFalse<T>, "Unsupported binary expression node");
  }
}

std::string PrintType(const Type& type) {
  return std::visit(
      Utils::Overload{
          [](const IntType&) -> std::string { return "int"; },
          [](const BoolType&) -> std::string { return "bool"; },
          [](const std::shared_ptr<FuncType>& func_type) -> std::string {
            assert(func_type != nullptr);
            std::string result = "func(";
            for (size_t i = 0; i < func_type->parameter_types.size(); ++i) {
              result += PrintType(func_type->parameter_types[i]);
              if (i + 1 < func_type->parameter_types.size()) {
                result += ", ";
              }
            }
            result += ")";

            if (func_type->return_type.has_value()) {
              result += " -> " + PrintType(*func_type->return_type);
            }

            return result;
          }},
      type);
}

int GetPrecedence(const Expression& expression) {
  return std::visit(
      Utils::Overload{
          [](const IdentifierExpression&) -> int { return kAtomPrecedence; },
          [](const LiteralExpression&) -> int { return kAtomPrecedence; },
          [](const FunctionCall&) -> int { return kAtomPrecedence; },
          []<UnaryExpressionNode Node>(const Node&) -> int { return kUnaryPrecedence; },
          []<BinaryExpressionNode Node>(const Node&) -> int {
            return GetBinaryPrecedence<Node>();
          }},
      expression.value);
}

bool IsBinaryExpression(const Expression& expression) {
  return std::visit(
      Utils::Overload{
          []<BinaryExpressionNode Node>(const Node&) -> bool { return true; },
          []<typename Node>(const Node&) -> bool { return false; }},
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
          []<UnaryExpressionNode Node>(const Node& node) -> std::string {
            assert(node.operand != nullptr);
            return std::string(GetUnaryOperatorToken<Node>()) +
                   PrintExpressionWithContext(
                       *node.operand,
                       kUnaryPrecedence,
                       true);
          },
          []<BinaryExpressionNode Node>(const Node& node) -> std::string {
            assert(node.left != nullptr);
            assert(node.right != nullptr);
            return PrintBinaryExpression(
                *node.left,
                GetBinaryOperatorToken<Node>(),
                *node.right,
                GetBinaryPrecedence<Node>());
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
