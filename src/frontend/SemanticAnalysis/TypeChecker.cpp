#include "SemanticAnalysis/TypeChecker.hpp"

#include <cassert>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "Utils/Overload.hpp"

namespace Parsing {

namespace {

bool AreTypesEqual(const Type& left_type, const Type& right_type);
std::string TypeToString(const Type& type);

bool AreTypesEqual(const Type& left_type, const Type& right_type) {
  return std::visit(
      Utils::Overload{
          [](const BoolType&, const BoolType&) {
            return true;
          },
          [](const IntType&, const IntType&) {
            return true;
          },
          [](const FuncType& left_function, const FuncType& right_function) {
            if (left_function.parameter_types.size() !=
                right_function.parameter_types.size()) {
              return false;
            }

            if ((left_function.return_type == nullptr) !=
                (right_function.return_type == nullptr)) {
              return false;
            }

            if (left_function.return_type != nullptr &&
                right_function.return_type != nullptr &&
                !AreTypesEqual(
                    *left_function.return_type,
                    *right_function.return_type)) {
              return false;
            }

            for (size_t i = 0; i < left_function.parameter_types.size(); ++i) {
              if (!AreTypesEqual(
                      left_function.parameter_types[i],
                      right_function.parameter_types[i])) {
                return false;
              }
            }

            return true;
          },
          [](const auto&, const auto&) {
            return false;
          }},
      left_type.type,
      right_type.type);
}

std::string TypeToString(const Type& type) {
  return std::visit(
      Utils::Overload{
          [](const BoolType&) -> std::string {
            return "bool";
          },
          [](const IntType&) -> std::string {
            return "int";
          },
          [](const FuncType& function_type) -> std::string {
            std::string result = "func(";
            for (size_t i = 0; i < function_type.parameter_types.size(); ++i) {
              if (i > 0) {
                result += ", ";
              }

              result += TypeToString(function_type.parameter_types[i]);
            }
            result += ")";

            if (function_type.return_type != nullptr) {
              result += " -> " + TypeToString(*function_type.return_type);
            } else {
              result += " -> void";
            }

            return result;
          }},
      type.type);
}

class TypeCheckerVisitor {
 public:
  TypeCheckerVisitor(
      const Program& program,
      const UseResolver& use_resolver,
      const SymbolTable& symbol_table)
      : program_(program),
        use_resolver_(use_resolver),
        symbol_table_(symbol_table) {}

  void Check() {
    if (symbol_table_.GetTable(&program_) == nullptr) {
      throw std::runtime_error(
          "Type checker requires a symbol table built for the same program");
    }

    CollectFunctionDeclarations(program_.top_statements);
    VisitStatements(program_.top_statements);
  }

 private:
  struct ResolvedSymbol {
    const ASTNode* definition_node;
    const SymbolData* symbol_data;
  };

  ResolvedSymbol ResolveUsedSymbol(
      const std::string& name,
      const ASTNode* use_node,
      const std::string& error_context) const {
    const ASTNode* definition_node = use_resolver_.GetUsedVarDef(name, use_node);
    if (definition_node == nullptr) {
      throw std::runtime_error(error_context + ": unknown symbol " + name);
    }

    const SymbolData* symbol_data = symbol_table_.GetSymbolInfo(name, definition_node);
    if (symbol_data == nullptr) {
      throw std::runtime_error("symbol table is incorrect for this program");
    }

    return ResolvedSymbol{definition_node, symbol_data};
  }

  const FunctionDeclarationStatement* GetFunctionDeclaration(
      const ASTNode* function_node) const {
    const auto function_it = function_declarations_.find(function_node);
    if (function_it == function_declarations_.end()) {
      throw std::runtime_error("symbol table is incorrect for this program");
    }

    return function_it->second;
  }

  template <typename OpType>
  bool IsSupportedExprOp(const Type& type) const {
    return std::visit(
        []<typename ValueType>(const ValueType&) {
          return Parsing::IsSupportedExprOp<OpType, ValueType>();
        },
        type.type);
  }

  Type RequireValueType(
      const Expression& expression,
      const std::string& error_context) {
    const std::optional<Type> evaluated_type = EvaluateExpressionType(expression);
    if (!evaluated_type.has_value()) {
      throw std::runtime_error(error_context + ": expression has no value");
    }

    return *evaluated_type;
  }

  void CollectFunctionDeclarations(const List<Statement>& statements) {
    for (size_t i = 0; i < statements.size(); ++i) {
      assert(statements[i] != nullptr);
      CollectFunctionDeclarations(*statements[i]);
    }
  }

  void CollectFunctionDeclarations(const Statement& statement) {
    std::visit(
        Utils::Overload{
            [this](const FunctionDeclarationStatement& function_declaration) {
              function_declarations_.emplace(&function_declaration, &function_declaration);
              assert(function_declaration.body != nullptr);
              CollectFunctionDeclarations(function_declaration.body->statements);
            },
            [this](const IfStatement& if_statement) {
              CollectFunctionDeclarations(if_statement);
            },
            [this](const Block& block) {
              CollectFunctionDeclarations(block.statements);
            },
            [](const auto&) {
            }},
        statement.value);
  }

  void CollectFunctionDeclarations(const IfStatement& if_statement) {
    assert(if_statement.true_block != nullptr);
    assert(if_statement.else_tail != nullptr);
    CollectFunctionDeclarations(if_statement.true_block->statements);
    CollectFunctionDeclarations(*if_statement.else_tail);
  }

  void CollectFunctionDeclarations(const ElseTail& else_tail) {
    if (else_tail.else_if != nullptr) {
      CollectFunctionDeclarations(*else_tail.else_if);
    }

    if (else_tail.else_block != nullptr) {
      CollectFunctionDeclarations(else_tail.else_block->statements);
    }
  }

  void VisitStatements(const List<Statement>& statements) {
    for (size_t i = 0; i < statements.size(); ++i) {
      assert(statements[i] != nullptr);
      VisitStatement(*statements[i]);
    }
  }

  void VisitStatement(const Statement& statement) {
    std::visit(
        Utils::Overload{
            [this](const DeclarationStatement& declaration) {
              VisitDeclarationStatement(declaration);
            },
            [this](const FunctionDeclarationStatement& function_declaration) {
              VisitFunctionDeclarationStatement(function_declaration);
            },
            [this](const AssignmentStatement& assignment) {
              VisitAssignmentStatement(assignment);
            },
            [this](const PrintStatement& print_statement) {
              VisitPrintStatement(print_statement);
            },
            [this](const IfStatement& if_statement) {
              VisitIfStatement(if_statement);
            },
            [this](const ReturnStatement& return_statement) {
              VisitReturnStatement(return_statement);
            },
            [this](const Expression& expression) {
              VisitExpressionStatement(expression);
            },
            [this](const Block& block) {
              VisitBlock(block);
            }},
        statement.value);
  }

  void VisitDeclarationStatement(const DeclarationStatement& declaration) {
    if (declaration.initializer == nullptr) {
      return;
    }

    const Type initializer_type =
        RequireValueType(*declaration.initializer, "Declaration initializer");
    if (!AreTypesEqual(initializer_type, declaration.type)) {
      throw std::runtime_error(
          "Type mismatch in declaration of " + declaration.variable_name +
          ": expected " + TypeToString(declaration.type) +
          ", got " + TypeToString(initializer_type));
    }
  }

  void VisitFunctionDeclarationStatement(
      const FunctionDeclarationStatement& function_declaration) {
    assert(function_declaration.body != nullptr);
    function_return_type_stack_.push_back(function_declaration.return_type);
    VisitStatements(function_declaration.body->statements);
    function_return_type_stack_.pop_back();
  }

  void VisitAssignmentStatement(const AssignmentStatement& assignment) {
    assert(assignment.expr != nullptr);

    const ResolvedSymbol resolved_symbol = ResolveUsedSymbol(
        assignment.variable_name,
        &assignment,
        "Assignment");
    const Type value_type = RequireValueType(*assignment.expr, "Assignment");
    if (!AreTypesEqual(value_type, resolved_symbol.symbol_data->type)) {
      throw std::runtime_error(
          "Type mismatch in assignment to " + assignment.variable_name +
          ": expected " + TypeToString(resolved_symbol.symbol_data->type) +
          ", got " + TypeToString(value_type));
    }
  }

  void VisitPrintStatement(const PrintStatement& print_statement) {
    assert(print_statement.expr != nullptr);
    RequireValueType(*print_statement.expr, "Print statement");
  }

  void VisitIfStatement(const IfStatement& if_statement) {
    assert(if_statement.condition != nullptr);
    assert(if_statement.true_block != nullptr);
    assert(if_statement.else_tail != nullptr);

    const Type condition_type = RequireValueType(*if_statement.condition, "If condition");
    if (!std::holds_alternative<BoolType>(condition_type.type)) {
      throw std::runtime_error(
          "If condition must be bool, got " + TypeToString(condition_type));
    }

    VisitBlock(*if_statement.true_block);
    VisitElseTail(*if_statement.else_tail);
  }

  void VisitElseTail(const ElseTail& else_tail) {
    if (else_tail.else_if != nullptr) {
      VisitIfStatement(*else_tail.else_if);
    }

    if (else_tail.else_block != nullptr) {
      VisitBlock(*else_tail.else_block);
    }
  }

  void VisitReturnStatement(const ReturnStatement& return_statement) {
    if (function_return_type_stack_.empty()) {
      throw std::runtime_error("Return statement is outside of function");
    }

    const std::optional<Type>& expected_type = function_return_type_stack_.back();
    if (!expected_type.has_value()) {
      if (return_statement.expr != nullptr) {
        throw std::runtime_error("Function without return type cannot return a value");
      }

      return;
    }

    if (return_statement.expr == nullptr) {
      throw std::runtime_error("Return expression is required");
    }

    const Type actual_type =
        RequireValueType(*return_statement.expr, "Return statement");
    if (!AreTypesEqual(actual_type, *expected_type)) {
      throw std::runtime_error(
          "Return type mismatch: expected " + TypeToString(*expected_type) +
          ", got " + TypeToString(actual_type));
    }
  }

  void VisitExpressionStatement(const Expression& expression) {
    EvaluateExpressionType(expression);
  }

  void VisitBlock(const Block& block) {
    VisitStatements(block.statements);
  }

  std::optional<Type> EvaluateExpressionType(const Expression& expression) {
    return std::visit(
        Utils::Overload{
            [this](const IdentifierExpression& identifier_expression)
                -> std::optional<Type> {
              const ResolvedSymbol resolved_symbol = ResolveUsedSymbol(
                  identifier_expression.name,
                  &identifier_expression,
                  "Identifier");
              return resolved_symbol.symbol_data->type;
            },
            [](const LiteralExpression& literal_expression) -> std::optional<Type> {
              return std::visit(
                  Utils::Overload{
                      [](int) -> std::optional<Type> {
                        return Type{IntType{}};
                      },
                      [](bool) -> std::optional<Type> {
                        return Type{BoolType{}};
                      }},
                  literal_expression.value);
            },
            [this](const FunctionCall& function_call) -> std::optional<Type> {
              return EvaluateFunctionCallType(function_call);
            },
            [this]<UnaryExpressionNode Node>(const Node& unary_expression)
                -> std::optional<Type> {
              return EvaluateUnaryExpressionType(unary_expression);
            },
            [this]<BinaryExpressionNode Node>(const Node& binary_expression)
                -> std::optional<Type> {
              return EvaluateBinaryExpressionType(binary_expression);
            }},
        expression.value);
  }

  template <UnaryExpressionNode Node>
  std::optional<Type> EvaluateUnaryExpressionType(const Node& unary_expression) {
    assert(unary_expression.operand != nullptr);
    const Type operand_type =
        RequireValueType(*unary_expression.operand, "Unary expression");
    if (!IsSupportedExprOp<Node>(operand_type)) {
      throw std::runtime_error(
          "Unary operation is not supported for type " + TypeToString(operand_type));
    }

    return operand_type;
  }

  template <BinaryExpressionNode Node>
  std::optional<Type> EvaluateBinaryExpressionType(const Node& binary_expression) {
    assert(binary_expression.left != nullptr);
    assert(binary_expression.right != nullptr);
    const Type left_type =
        RequireValueType(*binary_expression.left, "Binary expression left operand");
    const Type right_type =
        RequireValueType(*binary_expression.right, "Binary expression right operand");
    if (!AreTypesEqual(left_type, right_type)) {
      throw std::runtime_error(
          "Binary expression type mismatch: left is " + TypeToString(left_type) +
          ", right is " + TypeToString(right_type));
    }

    if (!IsSupportedExprOp<Node>(left_type)) {
      throw std::runtime_error(
          "Binary operation is not supported for type " + TypeToString(left_type));
    }

    // Comparison always return bool
    if constexpr (
        std::is_same_v<Node, LogicalAndExpression> ||
        std::is_same_v<Node, LogicalOrExpression> ||
        std::is_same_v<Node, EqualExpression> ||
        std::is_same_v<Node, NotEqualExpression> ||
        std::is_same_v<Node, LessExpression> ||
        std::is_same_v<Node, GreaterExpression> ||
        std::is_same_v<Node, LessEqualExpression> ||
        std::is_same_v<Node, GreaterEqualExpression>) {
      return Type{BoolType{}};
    }

    return left_type;
  }


  struct CallArgumentsCheckState {
    bool has_named_arguments = false;
    bool has_positional_arguments = false;
    std::set<size_t> used_parameter_indices;
    size_t positional_index = 0;
  };

  void TypeCheckNamedCallArgument(
      const NamedCallArgument& named_argument,
      const FunctionCall& function_call,
      const FuncType& function_type,
      const FunctionDeclarationStatement& function_declaration,
      CallArgumentsCheckState& state) {
    state.has_named_arguments = true;
    if (state.has_positional_arguments) {
      throw std::runtime_error(
          "Function call " + function_call.function_name +
          " mixes named and positional arguments");
    }

    // TODO: move to the function of finding argument index by name
    size_t parameter_index = function_type.parameter_types.size();
    for (size_t i = 0; i < function_declaration.parameters.size(); ++i) {
      if (function_declaration.parameters[i].name == named_argument.name) {
        parameter_index = i;
        break;
      }
    }

    // TODO: also move it to this function.
    if (parameter_index == function_type.parameter_types.size()) {
      throw std::runtime_error(
          "Unknown named argument " + named_argument.name +
          " for function " + function_call.function_name);
    }

    if (!state.used_parameter_indices.insert(parameter_index).second) {
      throw std::runtime_error(
          "Duplicate named argument " + named_argument.name +
          " for function " + function_call.function_name);
    }

    assert(named_argument.value != nullptr);
    const Type argument_type = RequireValueType(
        *named_argument.value,
        "Function named argument");
    const Type& expected_type = function_type.parameter_types[parameter_index];
    if (!AreTypesEqual(argument_type, expected_type)) {
      throw std::runtime_error(
          "Type mismatch in named argument " + named_argument.name +
          ": expected " + TypeToString(expected_type) +
          ", got " + TypeToString(argument_type));
    }
  }

  void TypeCheckPositionalCallArgument(
      const PositionalCallArgument& positional_argument,
      const FunctionCall& function_call,
      const FuncType& function_type,
      CallArgumentsCheckState& state) {
    state.has_positional_arguments = true;
    if (state.has_named_arguments) {
      throw std::runtime_error(
          "Function call " + function_call.function_name +
          " mixes named and positional arguments");
    }

    if (state.positional_index >= function_type.parameter_types.size()) {
      throw std::runtime_error(
          "Function " + function_call.function_name +
          " argument count mismatch");
    }

    assert(positional_argument.value != nullptr);
    const Type argument_type = RequireValueType(
        *positional_argument.value,
        "Function positional argument");
    const Type& expected_type = function_type.parameter_types[state.positional_index];
    if (!AreTypesEqual(argument_type, expected_type)) {
      throw std::runtime_error(
          "Type mismatch in positional argument " +
          std::to_string(state.positional_index) +
          ": expected " + TypeToString(expected_type) +
          ", got " + TypeToString(argument_type));
    }

    ++state.positional_index;
  }

  void TypeCheckCallArguments(
      const FunctionCall& function_call,
      const FuncType& function_type,
      const FunctionDeclarationStatement& function_declaration) {
    if (function_call.arguments.size() != function_type.parameter_types.size()) {
      throw std::runtime_error(
          "Function " + function_call.function_name +
          " argument count mismatch");
    }

    CallArgumentsCheckState state;
    for (size_t i = 0; i < function_call.arguments.size(); ++i) {
      assert(function_call.arguments[i] != nullptr);
      std::visit(
          Utils::Overload{
              [this,
               &function_call,
               &function_type,
               &function_declaration,
               &state](const NamedCallArgument& named_argument) {
                TypeCheckNamedCallArgument(
                    named_argument,
                    function_call,
                    function_type,
                    function_declaration,
                    state);
              },
              [this,
               &function_call,
               &function_type,
               &state](const PositionalCallArgument& positional_argument) {
                TypeCheckPositionalCallArgument(
                    positional_argument,
                    function_call,
                    function_type,
                    state);
              }},
          *function_call.arguments[i]);
    }
  }

  std::optional<Type> EvaluateFunctionCallType(const FunctionCall& function_call) {
    const ResolvedSymbol resolved_symbol = ResolveUsedSymbol(
        function_call.function_name,
        &function_call,
        "Function call");
    if (resolved_symbol.symbol_data->kind != SymbolKind::Function) {
      throw std::runtime_error(function_call.function_name + " is not a function");
    }

    const auto* function_type_ptr =
        std::get_if<FuncType>(&resolved_symbol.symbol_data->type.type);
    if (function_type_ptr == nullptr) {
      throw std::runtime_error(
          "symbol table is incorrect for this program");
    }

    const FuncType& function_type = *function_type_ptr;
    const FunctionDeclarationStatement* function_declaration =
        GetFunctionDeclaration(resolved_symbol.definition_node);
    TypeCheckCallArguments(
        function_call,
        function_type,
        *function_declaration);

    if (function_type.return_type == nullptr) {
      return std::nullopt;
    }

    return *function_type.return_type;
  }

  const Program& program_;
  const UseResolver& use_resolver_;
  const SymbolTable& symbol_table_;
  std::map<const ASTNode*, const FunctionDeclarationStatement*> function_declarations_;
  std::vector<std::optional<Type>> function_return_type_stack_;
};

}  // namespace

void CheckTypes(
    const Program& program,
    const UseResolver& use_resolver,
    const SymbolTable& symbol_table) {
  TypeCheckerVisitor visitor(program, use_resolver, symbol_table);
  visitor.Check();
}

}  // namespace Parsing
