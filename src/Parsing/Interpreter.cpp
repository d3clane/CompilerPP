#include "Parsing/Interpreter.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

#include "Utils/Overload.hpp"

namespace Parsing {
namespace {

int EvaluateValue(const ValueVariant& value, const InterpreterContext& context) {
  return std::visit(
      Utils::Overload{
          [](const NumberExpression& number) -> int {
            return number.value;
          },
          [&context](const IdentifierExpression& identifier) -> int {
            const auto it = context.variables.find(identifier.name);
            if (it == context.variables.end()) {
              throw std::runtime_error("Unknown variable: " + identifier.name);
            }
            return it->second;
          }},
      value);
}

bool EvaluateComparison(const ComparisonOperatorVariant& op, int left, int right) {
  return std::visit(
      Utils::Overload{
          [left, right](const EqualComparison&)        -> bool { return left == right; },
          [left, right](const NotEqualComparison&)     -> bool { return left != right; },
          [left, right](const LessComparison&)         -> bool { return left <  right; },
          [left, right](const GreaterComparison&)      -> bool { return left >  right; },
          [left, right](const LessEqualComparison&)    -> bool { return left <= right; },
          [left, right](const GreaterEqualComparison&) -> bool { return left >= right; }},
      op);
}

bool EvaluateBoolExpression(const BoolExpression& expression, const InterpreterContext& context) {
  assert(expression.left != nullptr);
  assert(expression.op != nullptr);
  assert(expression.right != nullptr);

  const int left_value = EvaluateValue(*expression.left, context);
  const int right_value = EvaluateValue(*expression.right, context);
  return EvaluateComparison(*expression.op, left_value, right_value);
}

void ExecuteBlock(const Block& block, InterpreterContext& context, std::ostream& output);

void ExecuteAssignment(const AssignmentStatement& assignment, InterpreterContext& context) {
  assert(assignment.value != nullptr);

  const auto it = context.variables.find(assignment.variable_name);
  if (it == context.variables.end()) {
    throw std::runtime_error("Variable is not declared before assignment: " + assignment.variable_name);
  }

  const int new_value = EvaluateValue(*assignment.value, context);
  context.variables[assignment.variable_name] = new_value;
}

void ExecutePrint(
    const PrintStatement& print_statement,
    InterpreterContext& context,
    std::ostream& output) {
  assert(print_statement.value != nullptr);

  const int print_value = EvaluateValue(*print_statement.value, context);
  output << print_value << "\n";
}

void ExecuteIf(
    const IfStatement& if_statement,
    InterpreterContext& context,
    std::ostream& output) {
  assert(if_statement.condition != nullptr);
  assert(if_statement.true_block != nullptr);
  assert(if_statement.false_block != nullptr);

  const bool condition_result = EvaluateBoolExpression(*if_statement.condition, context);
  if (condition_result) {
    ExecuteBlock(*if_statement.true_block, context, output);
  } else {
    ExecuteBlock(*if_statement.false_block, context, output);
  }
}

void ExecuteStatement(
    const StatementVariant& statement,
    InterpreterContext& context,
    std::ostream& output) {
  std::visit(
      Utils::Overload{
          [&context](const AssignmentStatement& assignment) {
            ExecuteAssignment(assignment, context);
          },
          [&context, &output](const PrintStatement& print_statement) {
            ExecutePrint(print_statement, context, output);
          },
          [&context, &output](const IfStatement& if_statement) {
            ExecuteIf(if_statement, context, output);
          }},
      statement);
}

void ExecuteBlock(const Block& block, InterpreterContext& context, std::ostream& output) {
  for (int i = 0; i < static_cast<int>(block.statements.size()); ++i) {
    assert(block.statements[i] != nullptr);
    ExecuteStatement(*block.statements[i], context, output);
  }
}

void ExecuteTopStatement(
    const TopStatementVariant& statement,
    InterpreterContext& context,
    std::ostream& output) {
  std::visit(
      Utils::Overload{
          [&context](const DeclarationStatement& declaration) {
            if (declaration.type_name != "int") {
              throw std::runtime_error("Unsupported type: " + declaration.type_name);
            }

            const auto [_, inserted] = context.variables.emplace(declaration.variable_name, 0);
            if (!inserted) {
              throw std::runtime_error("Variable already declared: " + declaration.variable_name);
            }
          },
          [&context, &output](const AssignmentStatement& assignment) {
            ExecuteAssignment(assignment, context);
          },
          [&context, &output](const PrintStatement& print_statement) {
            ExecutePrint(print_statement, context, output);
          },
          [&context, &output](const IfStatement& if_statement) {
            ExecuteIf(if_statement, context, output);
          }},
      statement);
}

}  // namespace

InterpreterContext Interpret(const Program& program, std::ostream& output) {
  InterpreterContext context;

  for (int i = 0; i < static_cast<int>(program.top_statements.size()); ++i) {
    assert(program.top_statements[i] != nullptr);
    ExecuteTopStatement(*program.top_statements[i], context, output);
  }

  return context;
}

InterpreterContext Interpret(const Program& program) {
  return Interpret(program, std::cout);
}

}  // namespace Parsing
