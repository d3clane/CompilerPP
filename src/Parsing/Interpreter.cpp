#include "Parsing/Interpreter.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

#include "Utils/Overload.hpp"

namespace Parsing {

namespace {

int EvaluateExpression(const Expression& expression, const InterpreterContext& context) {
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
          },
          [&context](const UnaryPlusExpression& unary_expression) -> int {
            assert(unary_expression.operand != nullptr);
            return EvaluateExpression(*unary_expression.operand, context);
          },
          [&context](const UnaryMinusExpression& unary_expression) -> int {
            assert(unary_expression.operand != nullptr);
            return -EvaluateExpression(*unary_expression.operand, context);
          },
          [&context](const AddExpression& add_expression) -> int {
            assert(add_expression.left != nullptr);
            assert(add_expression.right != nullptr);
            return EvaluateExpression(*add_expression.left, context) +
                   EvaluateExpression(*add_expression.right, context);
          },
          [&context](const SubtractExpression& subtract_expression) -> int {
            assert(subtract_expression.left != nullptr);
            assert(subtract_expression.right != nullptr);
            return EvaluateExpression(*subtract_expression.left, context) -
                   EvaluateExpression(*subtract_expression.right, context);
          },
          [&context](const MultiplyExpression& multiply_expression) -> int {
            assert(multiply_expression.left != nullptr);
            assert(multiply_expression.right != nullptr);
            return EvaluateExpression(*multiply_expression.left, context) *
                   EvaluateExpression(*multiply_expression.right, context);
          },
          [&context](const DivideExpression& divide_expression) -> int {
            assert(divide_expression.left != nullptr);
            assert(divide_expression.right != nullptr);

            const int divisor = EvaluateExpression(*divide_expression.right, context);
            if (divisor == 0) {
              throw std::runtime_error("Division by zero");
            }

            return EvaluateExpression(*divide_expression.left, context) / divisor;
          },
          [&context](const ModuloExpression& modulo_expression) -> int {
            assert(modulo_expression.left != nullptr);
            assert(modulo_expression.right != nullptr);

            const int divisor = EvaluateExpression(*modulo_expression.right, context);
            if (divisor == 0) {
              throw std::runtime_error("Modulo by zero");
            }

            return EvaluateExpression(*modulo_expression.left, context) % divisor;
          }},
      expression.value);
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

  const int left_value = EvaluateExpression(*expression.left, context);
  const int right_value = EvaluateExpression(*expression.right, context);
  return EvaluateComparison(*expression.op, left_value, right_value);
}

void ExecuteBlock(const Block& block, InterpreterContext& context, std::ostream& output);

void ExecuteAssignment(const AssignmentStatement& assignment, InterpreterContext& context) {
  assert(assignment.expr != nullptr);

  const auto it = context.variables.find(assignment.variable_name);
  if (it == context.variables.end()) {
    throw std::runtime_error("Variable is not declared before assignment: " + assignment.variable_name);
  }

  const int new_value = EvaluateExpression(*assignment.expr, context);
  context.variables[assignment.variable_name] = new_value;
}

void ExecutePrint(
    const PrintStatement& print_statement,
    InterpreterContext& context,
    std::ostream& output) {
  assert(print_statement.expr != nullptr);

  const int print_value = EvaluateExpression(*print_statement.expr, context);
  output << print_value << "\n";
}

void ExecuteIf(
    const IfStatement& if_statement,
    InterpreterContext& context,
    std::ostream& output) {
  assert(if_statement.condition != nullptr);
  assert(if_statement.true_block != nullptr);
  assert(if_statement.else_tail != nullptr);
  assert(!(if_statement.else_tail->else_if != nullptr &&
           if_statement.else_tail->else_block != nullptr));
  const bool condition_result = EvaluateBoolExpression(*if_statement.condition, context);
  if (condition_result) {
    ExecuteBlock(*if_statement.true_block, context, output);
    return;
  }

  if (if_statement.else_tail->else_if != nullptr) {
    ExecuteIf(*if_statement.else_tail->else_if, context, output);
    return;
  }

  if (if_statement.else_tail->else_block != nullptr) {
    ExecuteBlock(*if_statement.else_tail->else_block, context, output);
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
