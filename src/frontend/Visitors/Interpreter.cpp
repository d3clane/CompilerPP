#include "Visitors/Interpreter.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

#include "SemanticAnalysis/Resolver.hpp"
#include "SemanticAnalysis/StatementNumerizer.hpp"
#include "SemanticAnalysis/SymbolTable.hpp"
#include "Utils/Overload.hpp"

namespace Parsing {

namespace {

[[noreturn]] void ThrowFunctionCallNotSupported(const FunctionCall& function_call) {
  throw std::runtime_error(
      "Function calls are not supported by interpreter: " + function_call.function_name.name);
}

[[noreturn]] void ThrowFieldAccessNotSupported(const FieldAccess& field_access) {
  throw std::runtime_error(
      "Field access is not supported by interpreter: " +
      field_access.object_name.name + "." + field_access.field_name.name);
}

[[noreturn]] void ThrowMethodCallNotSupported(const MethodCall& method_call) {
  throw std::runtime_error(
      "Method calls are not supported by interpreter: " +
      method_call.object_name.name + "." + method_call.function_call.function_name.name);
}

[[noreturn]] void ThrowClassNotSupported(const std::string& class_name) {
  throw std::runtime_error(
      "Classes are not supported by interpreter: " + class_name);
}

bool EvaluateEquality(const RuntimeValue& left, const RuntimeValue& right, bool negated) {
  return left == right ? !negated : negated;
}

struct InterpreterRuntime {
  InterpreterContext& context;
  const UseResolver& use_resolver;
};

const ASTNode* ResolveVariableDefinitionNode(
    const std::string& name,
    const ASTNode* use_node,
    const UseResolver& use_resolver,
    const std::string& missing_symbol_error_prefix) {
  const ASTNode* definition_node = use_resolver.GetUsedVarDef(name, use_node);
  if (definition_node == nullptr) {
    throw std::runtime_error(missing_symbol_error_prefix + name);
  }

  return definition_node;
}

RuntimeValue EvaluateExpression(const Expression& expression, const InterpreterRuntime& runtime);

std::pair<RuntimeValue, RuntimeValue> EvaluateBinaryOperands(
    const BinaryOperationBase& binary_expression,
    const InterpreterRuntime& runtime) {
  assert(binary_expression.left != nullptr);
  assert(binary_expression.right != nullptr);
  return {
      EvaluateExpression(*binary_expression.left, runtime),
      EvaluateExpression(*binary_expression.right, runtime)};
}

RuntimeValue EvaluateExpression(const Expression& expression, const InterpreterRuntime& runtime) {
  return std::visit(
      Utils::Overload{
          [](const LiteralExpression& literal) -> RuntimeValue {
            return literal.value;
          },
          [&runtime](const IdentifierExpression& identifier) -> RuntimeValue {
            const ASTNode* definition_node = ResolveVariableDefinitionNode(
                identifier.name,
                &identifier,
                runtime.use_resolver,
                "Unknown variable: ");
            const auto variable_it = runtime.context.variables.find(definition_node);
            if (variable_it == runtime.context.variables.end()) {
              throw std::runtime_error("Unknown variable: " + identifier.name);
            }

            return variable_it->second;
          },
          [&runtime](const UnaryPlusExpression& unary_expression) -> RuntimeValue {
            assert(unary_expression.operand != nullptr);
            return RuntimeValue{
                std::get<int>(EvaluateExpression(*unary_expression.operand, runtime))};
          },
          [&runtime](const UnaryMinusExpression& unary_expression) -> RuntimeValue {
            assert(unary_expression.operand != nullptr);
            return RuntimeValue{
                -std::get<int>(EvaluateExpression(*unary_expression.operand, runtime))};
          },
          [&runtime](const UnaryNotExpression& unary_expression) -> RuntimeValue {
            assert(unary_expression.operand != nullptr);
            return RuntimeValue{
                !std::get<bool>(EvaluateExpression(*unary_expression.operand, runtime))};
          },
          [&runtime](const AddExpression& binary_expression) -> RuntimeValue {
            const auto [left_value, right_value] =
                EvaluateBinaryOperands(binary_expression, runtime);
            return RuntimeValue{std::get<int>(left_value) + std::get<int>(right_value)};
          },
          [&runtime](const SubtractExpression& binary_expression) -> RuntimeValue {
            const auto [left_value, right_value] =
                EvaluateBinaryOperands(binary_expression, runtime);
            return RuntimeValue{std::get<int>(left_value) - std::get<int>(right_value)};
          },
          [&runtime](const MultiplyExpression& binary_expression) -> RuntimeValue {
            const auto [left_value, right_value] =
                EvaluateBinaryOperands(binary_expression, runtime);
            return RuntimeValue{std::get<int>(left_value) * std::get<int>(right_value)};
          },
          [&runtime](const DivideExpression& binary_expression) -> RuntimeValue {
            const auto [left_value, right_value] =
                EvaluateBinaryOperands(binary_expression, runtime);
            const int divisor = std::get<int>(right_value);
            if (divisor == 0) {
              throw std::runtime_error("Division by zero");
            }

            return RuntimeValue{std::get<int>(left_value) / divisor};
          },
          [&runtime](const ModuloExpression& binary_expression) -> RuntimeValue {
            const auto [left_value, right_value] =
                EvaluateBinaryOperands(binary_expression, runtime);
            const int divisor = std::get<int>(right_value);
            if (divisor == 0) {
              throw std::runtime_error("Modulo by zero");
            }

            return RuntimeValue{std::get<int>(left_value) % divisor};
          },
          [&runtime](const LogicalAndExpression& binary_expression) -> RuntimeValue {
            assert(binary_expression.left != nullptr);
            assert(binary_expression.right != nullptr);

            const RuntimeValue left_value =
                EvaluateExpression(*binary_expression.left, runtime);
            if (!std::get<bool>(left_value)) {
              return RuntimeValue{false};
            }

            const RuntimeValue right_value =
                EvaluateExpression(*binary_expression.right, runtime);
            return RuntimeValue{std::get<bool>(right_value)};
          },
          [&runtime](const LogicalOrExpression& binary_expression) -> RuntimeValue {
            assert(binary_expression.left != nullptr);
            assert(binary_expression.right != nullptr);

            const RuntimeValue left_value =
                EvaluateExpression(*binary_expression.left, runtime);
            if (std::get<bool>(left_value)) {
              return RuntimeValue{true};
            }

            const RuntimeValue right_value =
                EvaluateExpression(*binary_expression.right, runtime);
            return RuntimeValue{std::get<bool>(right_value)};
          },
          [&runtime](const EqualExpression& binary_expression) -> RuntimeValue {
            const auto [left_value, right_value] =
                EvaluateBinaryOperands(binary_expression, runtime);
            return RuntimeValue{EvaluateEquality(left_value, right_value, false)};
          },
          [&runtime](const NotEqualExpression& binary_expression) -> RuntimeValue {
            const auto [left_value, right_value] =
                EvaluateBinaryOperands(binary_expression, runtime);
            return RuntimeValue{EvaluateEquality(left_value, right_value, true)};
          },
          [&runtime](const LessExpression& binary_expression) -> RuntimeValue {
            const auto [left_value, right_value] =
                EvaluateBinaryOperands(binary_expression, runtime);
            return RuntimeValue{std::get<int>(left_value) < std::get<int>(right_value)};
          },
          [&runtime](const GreaterExpression& binary_expression) -> RuntimeValue {
            const auto [left_value, right_value] =
                EvaluateBinaryOperands(binary_expression, runtime);
            return RuntimeValue{std::get<int>(left_value) > std::get<int>(right_value)};
          },
          [&runtime](const LessEqualExpression& binary_expression) -> RuntimeValue {
            const auto [left_value, right_value] =
                EvaluateBinaryOperands(binary_expression, runtime);
            return RuntimeValue{std::get<int>(left_value) <= std::get<int>(right_value)};
          },
          [&runtime](const GreaterEqualExpression& binary_expression) -> RuntimeValue {
            const auto [left_value, right_value] =
                EvaluateBinaryOperands(binary_expression, runtime);
            return RuntimeValue{std::get<int>(left_value) >= std::get<int>(right_value)};
          },
          [](const FunctionCall& function_call) -> RuntimeValue {
            ThrowFunctionCallNotSupported(function_call);
          },
          [](const FieldAccess& field_access) -> RuntimeValue {
            ThrowFieldAccessNotSupported(field_access);
          },
          [](const MethodCall& method_call) -> RuntimeValue {
            ThrowMethodCallNotSupported(method_call);
          }},
      expression.value);
}

void ExecuteBlock(const Block& block, InterpreterRuntime& runtime, std::ostream& output);

void ExecuteDeclaration(const DeclarationStatement& declaration, InterpreterRuntime& runtime) {
  if (runtime.context.variables.find(&declaration) != runtime.context.variables.end()) {
    throw std::runtime_error("Variable already declared: " + declaration.variable_name.name);
  }

  if (std::holds_alternative<IntType>(declaration.type->type)) {
    RuntimeValue value{0};
    if (declaration.initializer != nullptr) {
      value = EvaluateExpression(*declaration.initializer, runtime);
    }

    runtime.context.variables.emplace(&declaration, value);
    return;
  }

  if (std::holds_alternative<BoolType>(declaration.type->type)) {
    RuntimeValue value{false};
    if (declaration.initializer != nullptr) {
      value = EvaluateExpression(*declaration.initializer, runtime);
    }

    runtime.context.variables.emplace(&declaration, value);
    return;
  }

  throw std::runtime_error("Unsupported type in declaration");
}

void ExecuteAssignment(const AssignmentStatement& assignment, InterpreterRuntime& runtime) {
  assert(assignment.expr != nullptr);

  const ASTNode* definition_node = ResolveVariableDefinitionNode(
      assignment.variable_name.name,
      &assignment,
      runtime.use_resolver,
      "Variable is not declared before assignment: ");
  auto variable_it = runtime.context.variables.find(definition_node);
  if (variable_it == runtime.context.variables.end()) {
    throw std::runtime_error(
        "Variable is not declared before assignment: " + assignment.variable_name.name);
  }

  const RuntimeValue assigned_value = EvaluateExpression(*assignment.expr, runtime);
  variable_it->second = assigned_value;
}

void ExecutePrint(
    const PrintStatement& print_statement,
    InterpreterRuntime& runtime,
    std::ostream& output) {
  assert(print_statement.expr != nullptr);

  const RuntimeValue print_value = EvaluateExpression(*print_statement.expr, runtime);
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
    InterpreterRuntime& runtime,
    std::ostream& output) {
  assert(if_statement.condition != nullptr);
  assert(if_statement.true_block != nullptr);
  assert(if_statement.else_tail != nullptr);
  assert(!(if_statement.else_tail->else_if != nullptr &&
           if_statement.else_tail->else_block != nullptr));

  const RuntimeValue condition_value = EvaluateExpression(*if_statement.condition, runtime);
  const bool condition_result = std::get<bool>(condition_value);

  if (condition_result) {
    ExecuteBlock(*if_statement.true_block, runtime, output);
    return;
  }

  if (if_statement.else_tail->else_if != nullptr) {
    ExecuteIf(*if_statement.else_tail->else_if, runtime, output);
    return;
  }

  if (if_statement.else_tail->else_block != nullptr) {
    ExecuteBlock(*if_statement.else_tail->else_block, runtime, output);
  }
}

void ExecuteReturn(const ReturnStatement&) {
  throw std::runtime_error("Return statements are not supported by interpreter");
}

void ExecuteDelete(const DeleteStatement&) {
  throw std::runtime_error("Delete statements are not supported by interpreter");
}

void ExecuteStatement(
    const Statement& statement,
    InterpreterRuntime& runtime,
    std::ostream& output) {
  std::visit(
      Utils::Overload{
          [&runtime](const DeclarationStatement& declaration) {
            ExecuteDeclaration(declaration, runtime);
          },
          [](const FunctionDeclarationStatement&) {
            // Function bodies are not executed during declaration.
          },
          [](const ClassDeclarationStatement& class_declaration) {
            ThrowClassNotSupported(class_declaration.class_name.name);
          },
          [&runtime](const AssignmentStatement& assignment) {
            ExecuteAssignment(assignment, runtime);
          },
          [&runtime, &output](const PrintStatement& print_statement) {
            ExecutePrint(print_statement, runtime, output);
          },
          [](const DeleteStatement& delete_statement) {
            ExecuteDelete(delete_statement);
          },
          [&runtime, &output](const IfStatement& if_statement) {
            ExecuteIf(if_statement, runtime, output);
          },
          [](const ReturnStatement& return_statement) {
            ExecuteReturn(return_statement);
          },
          [&runtime](const Expression& expression) {
            static_cast<void>(EvaluateExpression(expression, runtime));
          },
          [&runtime, &output](const Block& block) {
            ExecuteBlock(block, runtime, output);
          }},
      statement.value);
}

void ExecuteBlock(const Block& block, InterpreterRuntime& runtime, std::ostream& output) {
  for (size_t i = 0; i < block.statements.size(); ++i) {
    assert(block.statements[i] != nullptr);
    ExecuteStatement(*block.statements[i], runtime, output);
  }
}

}  // namespace

InterpreterContext Interpret(const Program& program, std::ostream& output) {
  StatementNumerizer numerizer = BuildStatementNumerizer(program);
  SymbolTable symbol_table = BuildSymbolTable(program, std::move(numerizer));
  UseResolver use_resolver = BuildUseResolver(program, symbol_table);

  InterpreterContext context;
  InterpreterRuntime runtime{context, use_resolver};
  const FunctionDeclarationStatement* main_function = nullptr;

  for (size_t i = 0; i < program.top_statements.size(); ++i) {
    assert(program.top_statements[i] != nullptr);
    if (const auto* variable_declaration =
            std::get_if<DeclarationStatement>(&program.top_statements[i]->value);
        variable_declaration != nullptr) {
      ExecuteDeclaration(*variable_declaration, runtime);
      continue;
    }

    const auto* function_declaration =
        std::get_if<FunctionDeclarationStatement>(&program.top_statements[i]->value);
    if (function_declaration != nullptr) {
      if (function_declaration->function_name.name == "main") {
        main_function = function_declaration;
      }
      continue;
    }

    const auto* class_declaration =
        std::get_if<ClassDeclarationStatement>(&program.top_statements[i]->value);
    if (class_declaration != nullptr) {
      ThrowClassNotSupported(class_declaration->class_name.name);
    }

    throw std::runtime_error("Top-level statement is not declaration");
  }

  if (main_function != nullptr) {
    assert(main_function->body != nullptr);
    ExecuteBlock(*main_function->body, runtime, output);
  }

  return context;
}

InterpreterContext Interpret(const Program& program) {
  return Interpret(program, std::cout);
}

}  // namespace Parsing
