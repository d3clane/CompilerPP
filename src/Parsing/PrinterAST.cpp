#include "Parsing/PrinterAST.hpp"

#include <cassert>

#include "Utils/Overload.hpp"

namespace Parsing {
namespace {

std::string PrintComparisonOperator(const ComparisonOperatorVariant& op);

void AppendLine(std::string& output, int indent, const std::string& text);
void PrintExpressionTree(const Expression& expression, int indent, std::string& output);
void PrintBoolExpressionTree(const BoolExpression& bool_expression, int indent, std::string& output);
void PrintBlockTree(const Block& block, int indent, std::string& output);
void PrintElseTailTree(const ElseTail& else_tail, int indent, std::string& output);
void PrintIfStatementTree(const IfStatement& if_statement, int indent, std::string& output);
void PrintStatementTree(const StatementVariant& statement, int indent, std::string& output);
void PrintTopStatementTree(const TopStatementVariant& statement, int indent, std::string& output);

std::string PrintComparisonOperator(const ComparisonOperatorVariant& op) {
  return std::visit(
      Utils::Overload{
          [](const EqualComparison&) -> std::string        { return "=="; },
          [](const NotEqualComparison&) -> std::string     { return "!="; },
          [](const LessComparison&) -> std::string         { return "<" ; },
          [](const GreaterComparison&) -> std::string      { return ">" ; },
          [](const LessEqualComparison&) -> std::string    { return "<="; },
          [](const GreaterEqualComparison&) -> std::string { return ">="; }},
      op);
}

void AppendLine(std::string& output, int indent, const std::string& text) {
  for (int i = 0; i < indent; ++i) {
    output += "\t";
  }
  output += text + "\n";
}

void PrintExpressionTree(const Expression& expression, int indent, std::string& output) {
  std::visit(
      Utils::Overload{
          [&output, indent](const IdentifierExpression& identifier) {
            AppendLine(output, indent, "IdentifierExpression: " + identifier.name);
          },
          [&output, indent](const NumberExpression& number) {
            AppendLine(output, indent, "NumberExpression: " + std::to_string(number.value));
          },
          [&output, indent](const UnaryPlusExpression& unary_expression) {
            assert(unary_expression.operand != nullptr);
            AppendLine(output, indent, "UnaryPlusExpression:");
            PrintExpressionTree(*unary_expression.operand, indent + 1, output);
          },
          [&output, indent](const UnaryMinusExpression& unary_expression) {
            assert(unary_expression.operand != nullptr);
            AppendLine(output, indent, "UnaryMinusExpression:");
            PrintExpressionTree(*unary_expression.operand, indent + 1, output);
          },
          [&output, indent](const AddExpression& add_expression) {
            assert(add_expression.left != nullptr);
            assert(add_expression.right != nullptr);
            AppendLine(output, indent, "AddExpression:");
            PrintExpressionTree(*add_expression.left, indent + 1, output);
            PrintExpressionTree(*add_expression.right, indent + 1, output);
          },
          [&output, indent](const SubtractExpression& subtract_expression) {
            assert(subtract_expression.left != nullptr);
            assert(subtract_expression.right != nullptr);
            AppendLine(output, indent, "SubtractExpression:");
            PrintExpressionTree(*subtract_expression.left, indent + 1, output);
            PrintExpressionTree(*subtract_expression.right, indent + 1, output);
          },
          [&output, indent](const MultiplyExpression& multiply_expression) {
            assert(multiply_expression.left != nullptr);
            assert(multiply_expression.right != nullptr);
            AppendLine(output, indent, "MultiplyExpression:");
            PrintExpressionTree(*multiply_expression.left, indent + 1, output);
            PrintExpressionTree(*multiply_expression.right, indent + 1, output);
          },
          [&output, indent](const DivideExpression& divide_expression) {
            assert(divide_expression.left != nullptr);
            assert(divide_expression.right != nullptr);
            AppendLine(output, indent, "DivideExpression:");
            PrintExpressionTree(*divide_expression.left, indent + 1, output);
            PrintExpressionTree(*divide_expression.right, indent + 1, output);
          },
          [&output, indent](const ModuloExpression& modulo_expression) {
            assert(modulo_expression.left != nullptr);
            assert(modulo_expression.right != nullptr);
            AppendLine(output, indent, "ModuloExpression:");
            PrintExpressionTree(*modulo_expression.left, indent + 1, output);
            PrintExpressionTree(*modulo_expression.right, indent + 1, output);
          }},
      expression.value);
}

void PrintBoolExpressionTree(const BoolExpression& bool_expression, int indent, std::string& output) {
  assert(bool_expression.left != nullptr);
  assert(bool_expression.op != nullptr);
  assert(bool_expression.right != nullptr);

  AppendLine(output, indent, "BoolExpression:");
  AppendLine(output, indent + 1, "Left:");
  PrintExpressionTree(*bool_expression.left, indent + 2, output);
  AppendLine(output, indent + 1, "Operator: " + PrintComparisonOperator(*bool_expression.op));
  AppendLine(output, indent + 1, "Right:");
  PrintExpressionTree(*bool_expression.right, indent + 2, output);
}

void PrintBlockTree(const Block& block, int indent, std::string& output) {
  AppendLine(output, indent, "Block:");
  if (block.statements.empty()) {
    AppendLine(output, indent + 1, "<empty>");
    return;
  }

  for (int i = 0; i < static_cast<int>(block.statements.size()); ++i) {
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
  PrintBoolExpressionTree(*if_statement.condition, indent + 2, output);
  AppendLine(output, indent + 1, "TrueBlock:");
  PrintBlockTree(*if_statement.true_block, indent + 2, output);
  PrintElseTailTree(*if_statement.else_tail, indent + 1, output);
}

void PrintStatementTree(const StatementVariant& statement, int indent, std::string& output) {
  std::visit(
      Utils::Overload{
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
          }},
      statement);
}

void PrintTopStatementTree(const TopStatementVariant& statement, int indent, std::string& output) {
  std::visit(
      Utils::Overload{
          [&output, indent](const DeclarationStatement& declaration) {
            AppendLine(
                output,
                indent,
                "DeclarationStatement: " + declaration.variable_name + " " + declaration.type_name);
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
          }},
      statement);
}

}  // namespace

std::string PrintAstTree(const Program& program) {
  std::string result;
  AppendLine(result, 0, "Program:");
  if (program.top_statements.empty()) {
    AppendLine(result, 1, "<empty>");
    return result;
  }

  for (int i = 0; i < static_cast<int>(program.top_statements.size()); ++i) {
    assert(program.top_statements[i] != nullptr);
    PrintTopStatementTree(*program.top_statements[i], 1, result);
  }

  return result;
}

}  // namespace Parsing
