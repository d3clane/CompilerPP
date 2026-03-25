#include "Visitors/PrinterAST.hpp"

#include <cassert>

#include "Utils/Overload.hpp"

namespace Parsing {
namespace {

std::string PrintType(const Type& type);

void AppendLine(std::string& output, int indent, const std::string& text);
void PrintExpressionTree(const Expression& expression, int indent, std::string& output);
void PrintFunctionCallTree(const FunctionCall& function_call, int indent, std::string& output);
void PrintBlockTree(const Block& block, int indent, std::string& output);
void PrintElseTailTree(const ElseTail& else_tail, int indent, std::string& output);
void PrintIfStatementTree(const IfStatement& if_statement, int indent, std::string& output);
void PrintStatementTree(const Statement& statement, int indent, std::string& output);

template <typename T>
inline constexpr bool kAlwaysFalse = false;

template <typename T>
constexpr const char* GetExpressionNodeName() {
  if constexpr (std::same_as<T, UnaryPlusExpression>) {
    return "UnaryPlusExpression";
  } else if constexpr (std::same_as<T, UnaryMinusExpression>) {
    return "UnaryMinusExpression";
  } else if constexpr (std::same_as<T, UnaryNotExpression>) {
    return "UnaryNotExpression";
  } else if constexpr (std::same_as<T, AddExpression>) {
    return "AddExpression";
  } else if constexpr (std::same_as<T, SubtractExpression>) {
    return "SubtractExpression";
  } else if constexpr (std::same_as<T, MultiplyExpression>) {
    return "MultiplyExpression";
  } else if constexpr (std::same_as<T, DivideExpression>) {
    return "DivideExpression";
  } else if constexpr (std::same_as<T, ModuloExpression>) {
    return "ModuloExpression";
  } else if constexpr (std::same_as<T, LogicalAndExpression>) {
    return "LogicalAndExpression";
  } else if constexpr (std::same_as<T, LogicalOrExpression>) {
    return "LogicalOrExpression";
  } else if constexpr (std::same_as<T, EqualExpression>) {
    return "EqualExpression";
  } else if constexpr (std::same_as<T, NotEqualExpression>) {
    return "NotEqualExpression";
  } else if constexpr (std::same_as<T, LessExpression>) {
    return "LessExpression";
  } else if constexpr (std::same_as<T, GreaterExpression>) {
    return "GreaterExpression";
  } else if constexpr (std::same_as<T, LessEqualExpression>) {
    return "LessEqualExpression";
  } else if constexpr (std::same_as<T, GreaterEqualExpression>) {
    return "GreaterEqualExpression";
  } else {
    static_assert(kAlwaysFalse<T>, "Unsupported expression node");
  }
}

template <UnaryExpressionNode T>
void PrintUnaryExpressionTree(
    const T& unary_expression,
    int indent,
    std::string& output) {
  assert(unary_expression.operand != nullptr);
  AppendLine(output, indent, std::string(GetExpressionNodeName<T>()) + ":");
  PrintExpressionTree(*unary_expression.operand, indent + 1, output);
}

template <BinaryExpressionNode T>
void PrintBinaryExpressionTree(
    const T& binary_expression,
    int indent,
    std::string& output) {
  assert(binary_expression.left != nullptr);
  assert(binary_expression.right != nullptr);
  AppendLine(output, indent, std::string(GetExpressionNodeName<T>()) + ":");
  PrintExpressionTree(*binary_expression.left, indent + 1, output);
  PrintExpressionTree(*binary_expression.right, indent + 1, output);
}

std::string PrintType(const Type& type) {
  return std::visit(
      Utils::Overload{
          [](const IntType&) -> std::string { return "int"; },
          [](const BoolType&) -> std::string { return "bool"; },
          [](const FuncType& func_type) -> std::string {
            std::string result = "func(";
            for (size_t i = 0; i < func_type.parameter_types.size(); ++i) {
              result += PrintType(func_type.parameter_types[i]);
              if (i + 1 < func_type.parameter_types.size()) {
                result += ", ";
              }
            }
            result += ")";

            if (func_type.return_type != nullptr) {
              result += " -> " + PrintType(*func_type.return_type);
            }

            return result;
          }},
      type.type);
}

void AppendLine(std::string& output, int indent, const std::string& text) {
  for (int i = 0; i < indent; ++i) {
    output += "\t";
  }
  output += text + "\n";
}

void PrintFunctionCallTree(const FunctionCall& function_call, int indent, std::string& output) {
  AppendLine(output, indent, "FunctionCall: " + function_call.function_name);
  AppendLine(output, indent + 1, "Arguments:");

  if (function_call.arguments.empty()) {
    AppendLine(output, indent + 2, "<empty>");
    return;
  }

  for (size_t i = 0; i < function_call.arguments.size(); ++i) {
    assert(function_call.arguments[i] != nullptr);

    std::visit(
        Utils::Overload{
            [&output, indent](const NamedCallArgument& argument) {
              assert(argument.value != nullptr);
              AppendLine(output, indent + 2, "NamedArgument: " + argument.name);
              PrintExpressionTree(*argument.value, indent + 3, output);
            },
            [&output, indent](const PositionalCallArgument& argument) {
              assert(argument.value != nullptr);
              AppendLine(output, indent + 2, "PositionalArgument:");
              PrintExpressionTree(*argument.value, indent + 3, output);
            }},
        *function_call.arguments[i]);
  }
}

void PrintExpressionTree(const Expression& expression, int indent, std::string& output) {
  std::visit(
      Utils::Overload{
          [&output, indent](const IdentifierExpression& identifier) {
            AppendLine(output, indent, "IdentifierExpression: " + identifier.name);
          },
          [&output, indent](const LiteralExpression& literal) {
            const std::string value_text = std::visit(
                Utils::Overload{
                    [](int value) -> std::string { return std::to_string(value); },
                    [](bool value) -> std::string { return value ? "true" : "false"; }},
                literal.value);
            AppendLine(output, indent, "LiteralExpression: " + value_text);
          },
          [&output, indent](const FunctionCall& function_call) {
            AppendLine(output, indent, "FunctionCallExpression:");
            PrintFunctionCallTree(function_call, indent + 1, output);
          },
          [&output, indent]<UnaryExpressionNode Node>(const Node& node) {
            PrintUnaryExpressionTree(node, indent, output);
          },
          [&output, indent]<BinaryExpressionNode Node>(const Node& node) {
            PrintBinaryExpressionTree(node, indent, output);
          }},
      expression.value);
}

void PrintBlockTree(const Block& block, int indent, std::string& output) {
  AppendLine(output, indent, "Block:");
  if (block.statements.empty()) {
    AppendLine(output, indent + 1, "<empty>");
    return;
  }

  for (size_t i = 0; i < block.statements.size(); ++i) {
    assert(block.statements[i] != nullptr);
    PrintStatementTree(*block.statements[i], indent + 1, output);
  }
}

void PrintElseTailTree(const ElseTail& else_tail, int indent, std::string& output) {
  assert(!(else_tail.else_if != nullptr && else_tail.else_block != nullptr));

  AppendLine(output, indent, "ElseTail:");
  if (else_tail.else_if == nullptr && else_tail.else_block == nullptr) {
    AppendLine(output, indent + 1, "<empty>");
    return;
  }

  if (else_tail.else_if != nullptr) {
    AppendLine(output, indent + 1, "ElseIf:");
    PrintIfStatementTree(*else_tail.else_if, indent + 2, output);
    return;
  }

  assert(else_tail.else_block != nullptr);
  AppendLine(output, indent + 1, "ElseBlock:");
  PrintBlockTree(*else_tail.else_block, indent + 2, output);
}

void PrintIfStatementTree(const IfStatement& if_statement, int indent, std::string& output) {
  assert(if_statement.condition != nullptr);
  assert(if_statement.true_block != nullptr);
  assert(if_statement.else_tail != nullptr);

  AppendLine(output, indent, "IfStatement:");
  AppendLine(output, indent + 1, "Condition:");
  PrintExpressionTree(*if_statement.condition, indent + 2, output);
  AppendLine(output, indent + 1, "TrueBlock:");
  PrintBlockTree(*if_statement.true_block, indent + 2, output);
  PrintElseTailTree(*if_statement.else_tail, indent + 1, output);
}

void PrintStatementTree(const Statement& statement, int indent, std::string& output) {
  std::visit(
      Utils::Overload{
          [&output, indent](const DeclarationStatement& declaration) {
            std::string line = "DeclarationStatement: ";
            if (declaration.is_mutable) {
              line += "mutable ";
            }
            line += declaration.variable_name + " " + PrintType(declaration.type);
            AppendLine(output, indent, line);

            if (declaration.initializer != nullptr) {
              AppendLine(output, indent + 1, "Initializer:");
              PrintExpressionTree(*declaration.initializer, indent + 2, output);
            }
          },
          [&output, indent](const FunctionDeclarationStatement& function_declaration) {
            assert(function_declaration.body != nullptr);

            AppendLine(output, indent, "FunctionDeclarationStatement: " + function_declaration.function_name);

            AppendLine(output, indent + 1, "Parameters:");
            if (function_declaration.parameters.empty()) {
              AppendLine(output, indent + 2, "<empty>");
            } else {
              for (size_t i = 0; i < function_declaration.parameters.size(); ++i) {
                AppendLine(
                    output,
                    indent + 2,
                    "Parameter: " + function_declaration.parameters[i].name +
                        " " +
                        PrintType(function_declaration.parameters[i].type));
              }
            }

            if (!function_declaration.return_type.has_value()) {
              AppendLine(output, indent + 1, "ReturnType: <empty>");
            } else {
              AppendLine(output, indent + 1, "ReturnType: " + PrintType(*function_declaration.return_type));
            }

            AppendLine(output, indent + 1, "Body:");
            PrintBlockTree(*function_declaration.body, indent + 2, output);
          },
          [&output, indent](const AssignmentStatement& assignment) {
            assert(assignment.expr != nullptr);
            AppendLine(output, indent, "AssignmentStatement: " + assignment.variable_name);
            PrintExpressionTree(*assignment.expr, indent + 1, output);
          },
          [&output, indent](const PrintStatement& print_statement) {
            assert(print_statement.expr != nullptr);
            AppendLine(output, indent, "PrintStatement:");
            PrintExpressionTree(*print_statement.expr, indent + 1, output);
          },
          [&output, indent](const IfStatement& if_statement) {
            PrintIfStatementTree(if_statement, indent, output);
          },
          [&output, indent](const ReturnStatement& return_statement) {
            AppendLine(output, indent, "ReturnStatement:");
            if (return_statement.expr == nullptr) {
              AppendLine(output, indent + 1, "<empty>");
              return;
            }
            PrintExpressionTree(*return_statement.expr, indent + 1, output);
          },
          [&output, indent](const Expression& expression) {
            AppendLine(output, indent, "ExpressionStatement:");
            PrintExpressionTree(expression, indent + 1, output);
          },
          [&output, indent](const Block& block) {
            AppendLine(output, indent, "BlockStatement:");
            PrintBlockTree(block, indent + 1, output);
          }},
      statement.value);
}

}  // namespace

std::string PrintAstTree(const Program& program) {
  std::string result;
  AppendLine(result, 0, "Program:");
  if (program.top_statements.empty()) {
    AppendLine(result, 1, "<empty>");
    return result;
  }

  for (size_t i = 0; i < program.top_statements.size(); ++i) {
    assert(program.top_statements[i] != nullptr);
    PrintStatementTree(*program.top_statements[i], 1, result);
  }

  return result;
}

}  // namespace Parsing
