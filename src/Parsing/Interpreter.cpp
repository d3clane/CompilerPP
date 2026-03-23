#include "Parsing/Interpreter.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <variant>

#include "Utils/Overload.hpp"

namespace Parsing {

namespace {

[[noreturn]] void ThrowFunctionCallNotSupported(const FunctionCall& function_call) {
  throw std::runtime_error(
      "Function calls are not supported by interpreter: " + function_call.function_name);
}

[[noreturn]] void ThrowTypeError(const std::string& message) {
  throw std::runtime_error("Type error: " + message);
}

int RequireInt(const RuntimeValue& value, const std::string& context) {
  if (const auto* int_value = std::get_if<int>(&value); int_value != nullptr) {
    return *int_value;
  }

  ThrowTypeError(context + " requires int");
}

bool RequireBool(const RuntimeValue& value, const std::string& context) {
  if (const auto* bool_value = std::get_if<bool>(&value); bool_value != nullptr) {
    return *bool_value;
  }

  ThrowTypeError(context + " requires bool");
}

bool EvaluateEquality(const RuntimeValue& left, const RuntimeValue& right, bool negated) {
  if (left.index() != right.index()) {
    ThrowTypeError(std::string(negated ? "!=" : "==") + " requires operands of the same type");
  }
  return left == right ? !negated : negated;
}

RuntimeValue EvaluateExpression(const Expression& expression, const InterpreterContext& context);

RuntimeValue EvaluateExpression(const Expression& expression, const InterpreterContext& context) {
  return std::visit(
      Utils::Overload{
          [](const LiteralExpression& literal) -> RuntimeValue {
            return literal.value;
          },
          [&context](const IdentifierExpression& identifier) -> RuntimeValue {
            const auto variable_it = context.variables.find(identifier.name);
            if (variable_it != context.variables.end()) {
              return variable_it->second;
            }

            throw std::runtime_error("Unknown variable: " + identifier.name);
          },
          [&context](const UnaryPlusExpression& unary_expression) -> RuntimeValue {
            assert(unary_expression.operand != nullptr);
            const RuntimeValue operand_value = EvaluateExpression(*unary_expression.operand, context);
            return RuntimeValue{RequireInt(operand_value, "Unary +")};
          },
          [&context](const UnaryMinusExpression& unary_expression) -> RuntimeValue {
            assert(unary_expression.operand != nullptr);
            const RuntimeValue operand_value = EvaluateExpression(*unary_expression.operand, context);
            return RuntimeValue{-RequireInt(operand_value, "Unary -")};
          },
          [&context](const UnaryNotExpression& unary_expression) -> RuntimeValue {
            assert(unary_expression.operand != nullptr);
            const RuntimeValue operand_value = EvaluateExpression(*unary_expression.operand, context);
            return RuntimeValue{!RequireBool(operand_value, "Unary !")};
          },
          [&context](const AddExpression& add_expression) -> RuntimeValue {
            assert(add_expression.left != nullptr);
            assert(add_expression.right != nullptr);

            const RuntimeValue left_value = EvaluateExpression(*add_expression.left, context);
            const RuntimeValue right_value = EvaluateExpression(*add_expression.right, context);
            return RuntimeValue{RequireInt(left_value, "+") + RequireInt(right_value, "+")};
          },
          [&context](const SubtractExpression& subtract_expression) -> RuntimeValue {
            assert(subtract_expression.left != nullptr);
            assert(subtract_expression.right != nullptr);

            const RuntimeValue left_value = EvaluateExpression(*subtract_expression.left, context);
            const RuntimeValue right_value = EvaluateExpression(*subtract_expression.right, context);
            return RuntimeValue{RequireInt(left_value, "-") - RequireInt(right_value, "-")};
          },
          [&context](const MultiplyExpression& multiply_expression) -> RuntimeValue {
            assert(multiply_expression.left != nullptr);
            assert(multiply_expression.right != nullptr);

            const RuntimeValue left_value = EvaluateExpression(*multiply_expression.left, context);
            const RuntimeValue right_value = EvaluateExpression(*multiply_expression.right, context);
            return RuntimeValue{RequireInt(left_value, "*") * RequireInt(right_value, "*")};
          },
          [&context](const DivideExpression& divide_expression) -> RuntimeValue {
            assert(divide_expression.left != nullptr);
            assert(divide_expression.right != nullptr);

            const RuntimeValue left_value = EvaluateExpression(*divide_expression.left, context);
            const RuntimeValue right_value = EvaluateExpression(*divide_expression.right, context);
            const int divisor = RequireInt(right_value, "/");
            if (divisor == 0) {
              throw std::runtime_error("Division by zero");
            }

            return RuntimeValue{RequireInt(left_value, "/") / divisor};
          },
          [&context](const ModuloExpression& modulo_expression) -> RuntimeValue {
            assert(modulo_expression.left != nullptr);
            assert(modulo_expression.right != nullptr);

            const RuntimeValue left_value = EvaluateExpression(*modulo_expression.left, context);
            const RuntimeValue right_value = EvaluateExpression(*modulo_expression.right, context);
            const int divisor = RequireInt(right_value, "%");
            if (divisor == 0) {
              throw std::runtime_error("Modulo by zero");
            }

            return RuntimeValue{RequireInt(left_value, "%") % divisor};
          },
          [&context](const LogicalAndExpression& and_expression) -> RuntimeValue {
            assert(and_expression.left != nullptr);
            assert(and_expression.right != nullptr);

            const RuntimeValue left_value = EvaluateExpression(*and_expression.left, context);
            if (!RequireBool(left_value, "&&")) {
              return RuntimeValue{false};
            }

            const RuntimeValue right_value = EvaluateExpression(*and_expression.right, context);
            return RuntimeValue{RequireBool(right_value, "&&")};
          },
          [&context](const LogicalOrExpression& or_expression) -> RuntimeValue {
            assert(or_expression.left != nullptr);
            assert(or_expression.right != nullptr);

            const RuntimeValue left_value = EvaluateExpression(*or_expression.left, context);
            if (RequireBool(left_value, "||")) {
              return RuntimeValue{true};
            }

            const RuntimeValue right_value = EvaluateExpression(*or_expression.right, context);
            return RuntimeValue{RequireBool(right_value, "||")};
          },
          [&context](const EqualExpression& equal_expression) -> RuntimeValue {
            assert(equal_expression.left != nullptr);
            assert(equal_expression.right != nullptr);

            const RuntimeValue left_value = EvaluateExpression(*equal_expression.left, context);
            const RuntimeValue right_value = EvaluateExpression(*equal_expression.right, context);
            return RuntimeValue{EvaluateEquality(left_value, right_value, false)};
          },
          [&context](const NotEqualExpression& not_equal_expression) -> RuntimeValue {
            assert(not_equal_expression.left != nullptr);
            assert(not_equal_expression.right != nullptr);

            const RuntimeValue left_value = EvaluateExpression(*not_equal_expression.left, context);
            const RuntimeValue right_value = EvaluateExpression(*not_equal_expression.right, context);
            return RuntimeValue{EvaluateEquality(left_value, right_value, true)};
          },
          [&context](const LessExpression& less_expression) -> RuntimeValue {
            assert(less_expression.left != nullptr);
            assert(less_expression.right != nullptr);

            const RuntimeValue left_value = EvaluateExpression(*less_expression.left, context);
            const RuntimeValue right_value = EvaluateExpression(*less_expression.right, context);
            return RuntimeValue{RequireInt(left_value, "<") < RequireInt(right_value, "<")};
          },
          [&context](const GreaterExpression& greater_expression) -> RuntimeValue {
            assert(greater_expression.left != nullptr);
            assert(greater_expression.right != nullptr);

            const RuntimeValue left_value = EvaluateExpression(*greater_expression.left, context);
            const RuntimeValue right_value = EvaluateExpression(*greater_expression.right, context);
            return RuntimeValue{RequireInt(left_value, ">") > RequireInt(right_value, ">")};
          },
          [&context](const LessEqualExpression& less_equal_expression) -> RuntimeValue {
            assert(less_equal_expression.left != nullptr);
            assert(less_equal_expression.right != nullptr);

            const RuntimeValue left_value = EvaluateExpression(*less_equal_expression.left, context);
            const RuntimeValue right_value = EvaluateExpression(*less_equal_expression.right, context);
            return RuntimeValue{RequireInt(left_value, "<=") <= RequireInt(right_value, "<=")};
          },
          [&context](const GreaterEqualExpression& greater_equal_expression) -> RuntimeValue {
            assert(greater_equal_expression.left != nullptr);
            assert(greater_equal_expression.right != nullptr);

            const RuntimeValue left_value = EvaluateExpression(*greater_equal_expression.left, context);
            const RuntimeValue right_value = EvaluateExpression(*greater_equal_expression.right, context);
            return RuntimeValue{RequireInt(left_value, ">=") >= RequireInt(right_value, ">=")};
          },
          [](const FunctionCall& function_call) -> RuntimeValue {
            ThrowFunctionCallNotSupported(function_call);
          }},
      expression.value);
}

void ExecuteBlock(const Block& block, InterpreterContext& context, std::ostream& output);

void ExecuteDeclaration(const DeclarationStatement& declaration, InterpreterContext& context) {
  if (context.variables.find(declaration.variable_name) != context.variables.end()) {
    throw std::runtime_error("Variable already declared: " + declaration.variable_name);
  }

  if (std::holds_alternative<IntType>(declaration.type)) {
    RuntimeValue value{0};
    if (declaration.initializer != nullptr) {
      const RuntimeValue initializer_value = EvaluateExpression(*declaration.initializer, context);
      value = RuntimeValue{RequireInt(initializer_value, "int initializer")};
    }

    context.variables.emplace(declaration.variable_name, value);
    return;
  }

  if (std::holds_alternative<BoolType>(declaration.type)) {
    RuntimeValue value{false};
    if (declaration.initializer != nullptr) {
      const RuntimeValue initializer_value = EvaluateExpression(*declaration.initializer, context);
      value = RuntimeValue{RequireBool(initializer_value, "bool initializer")};
    }

    context.variables.emplace(declaration.variable_name, value);
    return;
  }

  throw std::runtime_error("Unsupported type in declaration");
}

void ExecuteAssignment(const AssignmentStatement& assignment, InterpreterContext& context) {
  assert(assignment.expr != nullptr);

  auto variable_it = context.variables.find(assignment.variable_name);
  if (variable_it == context.variables.end()) {
    throw std::runtime_error(
        "Variable is not declared before assignment: " + assignment.variable_name);
  }

  const RuntimeValue assigned_value = EvaluateExpression(*assignment.expr, context);
  if (std::holds_alternative<int>(variable_it->second)) {
    variable_it->second = RuntimeValue{RequireInt(assigned_value, "int assignment")};
    return;
  }

  variable_it->second = RuntimeValue{RequireBool(assigned_value, "bool assignment")};
}

void ExecutePrint(
    const PrintStatement& print_statement,
    InterpreterContext& context,
    std::ostream& output) {
  assert(print_statement.expr != nullptr);

  const RuntimeValue print_value = EvaluateExpression(*print_statement.expr, context);
  std::visit(
      Utils::Overload{
          [&output](int value) {
            output << value << "\n";
          },
          [&output](bool value) {
            output << (value ? "true" : "false") << "\n";
          }},
      print_value);
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

  const RuntimeValue condition_value = EvaluateExpression(*if_statement.condition, context);
  const bool condition_result = RequireBool(condition_value, "if condition");

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

void ExecuteReturn(const ReturnStatement&) {
  throw std::runtime_error("Return statements are not supported by interpreter");
}

void ExecuteStatement(
    const Statement& statement,
    InterpreterContext& context,
    std::ostream& output) {
  std::visit(
      Utils::Overload{
          [&context](const DeclarationStatement& declaration) {
            ExecuteDeclaration(declaration, context);
          },
          [](const FunctionDeclarationStatement&) {
            // Function bodies are not executed during declaration.
          },
          [&context](const AssignmentStatement& assignment) {
            ExecuteAssignment(assignment, context);
          },
          [&context, &output](const PrintStatement& print_statement) {
            ExecutePrint(print_statement, context, output);
          },
          [&context, &output](const IfStatement& if_statement) {
            ExecuteIf(if_statement, context, output);
          },
          [](const ReturnStatement& return_statement) {
            ExecuteReturn(return_statement);
          },
          [&context](const Expression& expression) {
            static_cast<void>(EvaluateExpression(expression, context));
          },
          [&context, &output](const Block& block) {
            ExecuteBlock(block, context, output);
          }},
      statement.value);
}

void ExecuteBlock(const Block& block, InterpreterContext& context, std::ostream& output) {
  for (size_t i = 0; i < block.statements.size(); ++i) {
    assert(block.statements[i] != nullptr);
    ExecuteStatement(*block.statements[i], context, output);
  }
}

}  // namespace

InterpreterContext Interpret(const Program& program, std::ostream& output) {
  InterpreterContext context;

  for (size_t i = 0; i < program.top_statements.size(); ++i) {
    assert(program.top_statements[i] != nullptr);
    ExecuteStatement(*program.top_statements[i], context, output);
  }

  return context;
}

InterpreterContext Interpret(const Program& program) {
  return Interpret(program, std::cout);
}

}  // namespace Parsing
