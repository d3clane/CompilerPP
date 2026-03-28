#include "SemanticAnalysis/TypeChecker.hpp"

#include <cassert>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "Debug/DebugCtx.hpp"
#include "SemanticAnalysis/StatementNumerizer.hpp"
#include "Utils/Overload.hpp"

namespace Parsing {

namespace {

std::string TypeToString(const Type& type);

std::string TypeToString(const Type& type) {
  return std::visit(
      Utils::Overload{
          [](const BoolType&) -> std::string {
            return "bool";
          },
          [](const IntType&) -> std::string {
            return "int";
          },
          [](const ClassType& class_type) -> std::string {
            return class_type.class_name;
          },
          [](const ArrayType& array_type) -> std::string {
            if (array_type.element_type == nullptr) {
              return "<invalid>[]";
            }

            return TypeToString(*array_type.element_type) + "[]";
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
      const TypeDefiner& type_definer,
      DebugCtx& debug_ctx)
      : program_(program),
        use_resolver_(use_resolver),
        type_definer_(type_definer),
        debug_ctx_(debug_ctx),
        numerizer_(BuildStatementNumerizer(program)) {}

  void Check() {
    VisitStatements(program_.top_statements);
  }

 private:
  void ReportCodeError(
      const ASTNode* node,
      const std::string& message) {
    debug_ctx_.GetErrors().AddError(node, message);
    recovering_from_error_ = true;
  }

  const UseResolver::ResolvedSymbol* ResolveUsedSymbol(
      const std::string& name,
      const ASTNode* use_node,
      const std::string& error_context) {
    const UseResolver::ResolvedSymbol* resolved_symbol =
        use_resolver_.GetResolvedSymbol(name, use_node);
    if (resolved_symbol == nullptr) {
      throw std::runtime_error(
          "resolver is incorrect for this program: " +
          error_context + ": unknown symbol " + name);
    }

    return resolved_symbol;
  }

  const FunctionDeclarationStatement* GetFunctionDeclaration(
      const ASTNode* function_node) const {
    assert(function_node != nullptr);
    return static_cast<const FunctionDeclarationStatement*>(function_node);
  }

  template <typename OpType>
  bool IsSupportedExprOp(const Type& type) const {
    return std::visit(
        []<typename ValueType>(const ValueType&) {
          return Parsing::IsSupportedExprOp<OpType, ValueType>();
        },
        type.type);
  }

  bool IsDerivedClassOf(
      const std::string& derived_class_name,
      const std::string& base_class_name) const {
    if (derived_class_name == base_class_name) {
      return true;
    }

    const ClassDeclarationStatement* current_class =
        type_definer_.GetClassDeclaration(derived_class_name);
    std::set<std::string> visited_classes;
    while (current_class != nullptr) {
      if (!visited_classes.insert(current_class->class_name).second) {
        return false;
      }

      if (!current_class->base_class_name.has_value()) {
        return false;
      }

      if (*current_class->base_class_name == base_class_name) {
        return true;
      }

      current_class =
          type_definer_.GetClassDeclaration(*current_class->base_class_name);
    }

    return false;
  }

  bool IsTypeAssignable(
      const Type& expected_type,
      const Type& actual_type) const {
    if (expected_type == actual_type) {
      return true;
    }

    const auto* expected_class_type = std::get_if<ClassType>(&expected_type.type);
    const auto* actual_class_type = std::get_if<ClassType>(&actual_type.type);
    if (expected_class_type == nullptr || actual_class_type == nullptr) {
      return false;
    }

    return IsDerivedClassOf(
        actual_class_type->class_name,
        expected_class_type->class_name);
  }

  bool IsClassTypeVisible(
      const ClassDeclarationStatement& class_declaration,
      const ASTNode* use_node) const {
    const std::optional<StatementNumerizer::ScopedStmtRef> declaration_ref =
        numerizer_.GetRef(&class_declaration);
    if (!declaration_ref.has_value()) {
      throw std::runtime_error("statement numerizer is incorrect for this program");
    }

    const std::optional<StatementNumerizer::ScopedStmtRef> use_ref =
        numerizer_.GetRef(use_node);
    if (!use_ref.has_value()) {
      throw std::runtime_error("statement numerizer is incorrect for this program");
    }

    const ASTNode* declaration_scope_owner =
        numerizer_.GetScopeOwnerFromRef(*declaration_ref);
    if (declaration_scope_owner == nullptr) {
      throw std::runtime_error("statement numerizer is incorrect for this program");
    }

    // Forward class use is allowed in global scope
    if (declaration_scope_owner == &program_) {
      return true;
    }

    const std::optional<StatementNumerizer::ScopedStmtRef> projected_use_ref =
        numerizer_.ProjectUseToScope(*use_ref, declaration_scope_owner);
    if (!projected_use_ref.has_value()) {
      return false;
    }

    return declaration_ref->stmt_id_in_scope <
           projected_use_ref->stmt_id_in_scope;
  }

  std::optional<Type> RequireValueType(
      const Expression& expression,
      const std::string& error_context) {
    const std::optional<Type> evaluated_type = EvaluateExpressionType(expression);
    if (!evaluated_type.has_value()) {
      if (!recovering_from_error_) {
        ReportCodeError(&expression, error_context + ": expression has no value");
      }
      return std::nullopt;
    }

    return evaluated_type;
  }

  void ValidateTypeReferences(
      const Type& type,
      const ASTNode* node,
      const std::string& error_context) {
    if (recovering_from_error_) {
      return;
    }

    std::visit(
        Utils::Overload{
            [](const IntType&) {},
            [](const BoolType&) {},
            [this, node, &error_context](const ClassType& class_type) {
              const ClassDeclarationStatement* class_declaration =
                  type_definer_.GetClassDeclaration(class_type.class_name);
              if (class_declaration == nullptr ||
                  !IsClassTypeVisible(*class_declaration, node)) {
                ReportCodeError(
                    node,
                    error_context + ": unknown class type " + class_type.class_name);
              }
            },
            [this, node, &error_context](const ArrayType& array_type) {
              if (array_type.element_type == nullptr) {
                throw std::runtime_error("invalid array type");
              }

              ValidateTypeReferences(*array_type.element_type, node, error_context);
            },
            [this, node, &error_context](const FuncType& function_type) {
              for (size_t i = 0; i < function_type.parameter_types.size(); ++i) {
                ValidateTypeReferences(
                    function_type.parameter_types[i],
                    node,
                    error_context);
                if (recovering_from_error_) {
                  return;
                }
              }

              if (function_type.return_type != nullptr) {
                ValidateTypeReferences(
                    *function_type.return_type,
                    node,
                    error_context);
              }
            }},
        type.type);
  }

  void VisitStatements(const List<Statement>& statements) {
    for (size_t i = 0; i < statements.size(); ++i) {
      assert(statements[i] != nullptr);
      recovering_from_error_ = false;
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
            [this](const ClassDeclarationStatement& class_declaration) {
              VisitClassDeclarationStatement(class_declaration);
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
    ValidateTypeReferences(
        declaration.type,
        &declaration,
        "Declaration type");
    if (recovering_from_error_) {
      return;
    }

    if (declaration.initializer == nullptr) {
      return;
    }

    const std::optional<Type> initializer_type =
        RequireValueType(*declaration.initializer, "Declaration initializer");
    if (recovering_from_error_ || !initializer_type.has_value()) {
      return;
    }

    if (!IsTypeAssignable(declaration.type, *initializer_type)) {
      ReportCodeError(
          &declaration,
          "Type mismatch in declaration of " + declaration.variable_name +
          ": expected " + TypeToString(declaration.type) +
          ", got " + TypeToString(*initializer_type));
    }
  }

  void VisitFunctionDeclarationStatementBody(
      const FunctionDeclarationStatement& function_declaration) {
    assert(function_declaration.body != nullptr);

    for (size_t i = 0; i < function_declaration.parameters.size(); ++i) {
      ValidateTypeReferences(
          function_declaration.parameters[i].type,
          &function_declaration.parameters[i],
          "Function parameter type");
      if (recovering_from_error_) {
        return;
      }
    }

    if (function_declaration.return_type.has_value()) {
      ValidateTypeReferences(
          *function_declaration.return_type,
          &function_declaration,
          "Function return type");
      if (recovering_from_error_) {
        return;
      }
    }

    function_return_type_stack_.push_back(function_declaration.return_type);
    VisitStatements(function_declaration.body->statements);
    function_return_type_stack_.pop_back();
  }

  void VisitFunctionDeclarationStatement(
      const FunctionDeclarationStatement& function_declaration) {
    const ClassDeclarationStatement* previous_method_owner_class =
        current_method_owner_class_;
    current_method_owner_class_ = nullptr;
    VisitFunctionDeclarationStatementBody(function_declaration);
    current_method_owner_class_ = previous_method_owner_class;
  }

  void VisitMethodDeclarationStatement(
      const FunctionDeclarationStatement& method_declaration,
      const ClassDeclarationStatement& class_declaration) {
    const ClassDeclarationStatement* previous_method_owner_class =
        current_method_owner_class_;
    current_method_owner_class_ = &class_declaration;
    VisitFunctionDeclarationStatementBody(method_declaration);
    current_method_owner_class_ = previous_method_owner_class;
  }

  void VisitClassDeclarationStatement(
      const ClassDeclarationStatement& class_declaration) {
    if (class_declaration.base_class_name.has_value()) {
      if (type_definer_.GetClassDeclaration(*class_declaration.base_class_name) == nullptr) {
        ReportCodeError(
            &class_declaration,
            "Unknown base class: " + *class_declaration.base_class_name);
      }
    }

    for (size_t i = 0; i < class_declaration.fields.size(); ++i) {
      recovering_from_error_ = false;
      VisitDeclarationStatement(class_declaration.fields[i]);
    }

    for (size_t i = 0; i < class_declaration.methods.size(); ++i) {
      recovering_from_error_ = false;
      VisitMethodDeclarationStatement(class_declaration.methods[i], class_declaration);
    }
  }

  void VisitAssignmentStatement(const AssignmentStatement& assignment) {
    assert(assignment.expr != nullptr);

    const UseResolver::ResolvedSymbol* resolved_symbol = ResolveUsedSymbol(
        assignment.variable_name,
        &assignment,
        "Assignment");
    if (recovering_from_error_) {
      return;
    }

    const std::optional<Type> value_type =
        RequireValueType(*assignment.expr, "Assignment");
    if (recovering_from_error_ || !value_type.has_value()) {
      return;
    }

    if (!IsTypeAssignable(resolved_symbol->symbol_data.type, *value_type)) {
      ReportCodeError(
          &assignment,
          "Type mismatch in assignment to " + assignment.variable_name +
          ": expected " + TypeToString(resolved_symbol->symbol_data.type) +
          ", got " + TypeToString(*value_type));
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

    const std::optional<Type> condition_type =
        RequireValueType(*if_statement.condition, "If condition");
    if (recovering_from_error_ || !condition_type.has_value()) {
      return;
    }

    if (!std::holds_alternative<BoolType>(condition_type->type)) {
      ReportCodeError(
          &if_statement,
          "If condition must be bool, got " + TypeToString(*condition_type));
      return;
    }

    VisitBlock(*if_statement.true_block);
    if (recovering_from_error_) {
      return;
    }

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
      ReportCodeError(&return_statement, "Return statement is outside of function");
      return;
    }

    const std::optional<Type>& expected_type = function_return_type_stack_.back();
    if (!expected_type.has_value()) {
      if (return_statement.expr != nullptr) {
        ReportCodeError(
            &return_statement,
            "Function without return type cannot return a value");
      }

      return;
    }

    if (return_statement.expr == nullptr) {
      ReportCodeError(&return_statement, "Return expression is required");
      return;
    }

    const std::optional<Type> actual_type =
        RequireValueType(*return_statement.expr, "Return statement");
    if (recovering_from_error_ || !actual_type.has_value()) {
      return;
    }

    if (!IsTypeAssignable(*expected_type, *actual_type)) {
      ReportCodeError(
          &return_statement,
          "Return type mismatch: expected " + TypeToString(*expected_type) +
          ", got " + TypeToString(*actual_type));
    }
  }

  void VisitExpressionStatement(const Expression& expression) {
    EvaluateExpressionType(expression);
  }

  void VisitBlock(const Block& block) {
    VisitStatements(block.statements);
  }

  std::optional<Type> EvaluateExpressionType(const Expression& expression) {
    if (recovering_from_error_) {
      return std::nullopt;
    }

    return std::visit(
        Utils::Overload{
            [this](const IdentifierExpression& identifier_expression)
                -> std::optional<Type> {
              const UseResolver::ResolvedSymbol* resolved_symbol = ResolveUsedSymbol(
                  identifier_expression.name,
                  &identifier_expression,
                  "Identifier");
              return resolved_symbol->symbol_data.type;
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
            [this](const FieldAccess& field_access) -> std::optional<Type> {
              return EvaluateFieldAccessType(field_access);
            },
            [this](const MethodCall& method_call) -> std::optional<Type> {
              return EvaluateMethodCallType(method_call);
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
    const std::optional<Type> operand_type =
        RequireValueType(*unary_expression.operand, "Unary expression");
    if (recovering_from_error_ || !operand_type.has_value()) {
      return std::nullopt;
    }

    if (!IsSupportedExprOp<Node>(*operand_type)) {
      ReportCodeError(
          &unary_expression,
          "Unary operation is not supported for type " + TypeToString(*operand_type));
      return std::nullopt;
    }

    return operand_type;
  }

  template <BinaryExpressionNode Node>
  std::optional<Type> EvaluateBinaryExpressionType(const Node& binary_expression) {
    assert(binary_expression.left != nullptr);
    assert(binary_expression.right != nullptr);
    const std::optional<Type> left_type =
        RequireValueType(*binary_expression.left, "Binary expression left operand");
    if (recovering_from_error_ || !left_type.has_value()) {
      return std::nullopt;
    }

    const std::optional<Type> right_type =
        RequireValueType(*binary_expression.right, "Binary expression right operand");
    if (recovering_from_error_ || !right_type.has_value()) {
      return std::nullopt;
    }

    if (*left_type != *right_type) {
      ReportCodeError(
          &binary_expression,
          "Binary expression type mismatch: left is " + TypeToString(*left_type) +
          ", right is " + TypeToString(*right_type));
      return std::nullopt;
    }

    if (!IsSupportedExprOp<Node>(*left_type)) {
      ReportCodeError(
          &binary_expression,
          "Binary operation is not supported for type " + TypeToString(*left_type));
      return std::nullopt;
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
      ReportCodeError(
          &function_call,
          "Function call " + function_call.function_name +
          " mixes named and positional arguments");
      return;
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
      ReportCodeError(
          &named_argument,
          "Unknown named argument " + named_argument.name +
          " for function " + function_call.function_name);
      return;
    }

    if (!state.used_parameter_indices.insert(parameter_index).second) {
      ReportCodeError(
          &named_argument,
          "Duplicate named argument " + named_argument.name +
          " for function " + function_call.function_name);
      return;
    }

    assert(named_argument.value != nullptr);
    const std::optional<Type> argument_type = RequireValueType(
        *named_argument.value,
        "Function named argument");
    if (recovering_from_error_ || !argument_type.has_value()) {
      return;
    }

    const Type& expected_type = function_type.parameter_types[parameter_index];
    if (!IsTypeAssignable(expected_type, *argument_type)) {
      ReportCodeError(
          &named_argument,
          "Type mismatch in named argument " + named_argument.name +
          ": expected " + TypeToString(expected_type) +
          ", got " + TypeToString(*argument_type));
    }
  }

  void TypeCheckPositionalCallArgument(
      const PositionalCallArgument& positional_argument,
      const FunctionCall& function_call,
      const FuncType& function_type,
      CallArgumentsCheckState& state) {
    state.has_positional_arguments = true;
    if (state.has_named_arguments) {
      ReportCodeError(
          &function_call,
          "Function call " + function_call.function_name +
          " mixes named and positional arguments");
      return;
    }

    if (state.positional_index >= function_type.parameter_types.size()) {
      ReportCodeError(
          &function_call,
          "Function " + function_call.function_name +
          " argument count mismatch");
      return;
    }

    assert(positional_argument.value != nullptr);
    const std::optional<Type> argument_type = RequireValueType(
        *positional_argument.value,
        "Function positional argument");
    if (recovering_from_error_ || !argument_type.has_value()) {
      return;
    }

    const Type& expected_type = function_type.parameter_types[state.positional_index];
    if (!IsTypeAssignable(expected_type, *argument_type)) {
      ReportCodeError(
          &positional_argument,
          "Type mismatch in positional argument " +
          std::to_string(state.positional_index) +
          ": expected " + TypeToString(expected_type) +
          ", got " + TypeToString(*argument_type));
      return;
    }

    ++state.positional_index;
  }

  void TypeCheckCallArguments(
      const FunctionCall& function_call,
      const FuncType& function_type,
      const FunctionDeclarationStatement& function_declaration) {
    if (function_call.arguments.size() != function_type.parameter_types.size()) {
      ReportCodeError(
          &function_call,
          "Function " + function_call.function_name +
          " argument count mismatch");
      return;
    }

    CallArgumentsCheckState state;
    for (size_t i = 0; i < function_call.arguments.size(); ++i) {
      if (recovering_from_error_) {
        return;
      }

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

  std::optional<Type> EvaluateFieldAccessType(const FieldAccess& field_access) {
    const UseResolver::ResolvedSymbol* resolved_receiver = ResolveUsedSymbol(
        field_access.object_name.name,
        &field_access.object_name,
        "Field access receiver");
    if (recovering_from_error_) {
      return std::nullopt;
    }

    const auto* receiver_class_type =
        std::get_if<ClassType>(&resolved_receiver->symbol_data.type.type);
    if (receiver_class_type == nullptr) {
      ReportCodeError(
          &field_access,
          "Field access receiver is not a class object: " + field_access.object_name.name);
      return std::nullopt;
    }

    if (current_method_owner_class_ == nullptr) {
      ReportCodeError(
          &field_access,
          "Field access is allowed only inside class methods");
      return std::nullopt;
    }

    if (receiver_class_type->class_name != current_method_owner_class_->class_name) {
      ReportCodeError(
          &field_access,
          "Field access is allowed only for objects of the current class type");
      return std::nullopt;
    }

    const UseResolver::ResolvedSymbol* resolved_field = ResolveUsedSymbol(
        field_access.field_name.name,
        &field_access.field_name,
        "Field access");
    if (recovering_from_error_) {
      return std::nullopt;
    }

    if (resolved_field->symbol_data.kind != SymbolKind::Variable) {
      throw std::runtime_error("resolver is incorrect for this program");
    }

    return resolved_field->symbol_data.type;
  }

  std::optional<Type> EvaluateMethodCallType(const MethodCall& method_call) {
    const UseResolver::ResolvedSymbol* resolved_receiver = ResolveUsedSymbol(
        method_call.object_name.name,
        &method_call.object_name,
        "Method call receiver");
    if (recovering_from_error_) {
      return std::nullopt;
    }

    const auto* receiver_class_type =
        std::get_if<ClassType>(&resolved_receiver->symbol_data.type.type);
    if (receiver_class_type == nullptr) {
      ReportCodeError(
          &method_call,
          "Method call receiver is not a class object: " + method_call.object_name.name);
      return std::nullopt;
    }

    const UseResolver::ResolvedSymbol* resolved_method = ResolveUsedSymbol(
        method_call.function_call.function_name,
        &method_call.function_call,
        "Method call");
    if (recovering_from_error_) {
      return std::nullopt;
    }

    if (resolved_method->symbol_data.kind != SymbolKind::Function) {
      throw std::runtime_error("resolver is incorrect for this program");
    }

    const auto* method_type_ptr =
        std::get_if<FuncType>(&resolved_method->symbol_data.type.type);
    if (method_type_ptr == nullptr) {
      throw std::runtime_error("resolver is incorrect for this program");
    }

    const FuncType& method_type = *method_type_ptr;
    const FunctionDeclarationStatement* method_declaration =
        GetFunctionDeclaration(resolved_method->definition_node);
    TypeCheckCallArguments(method_call.function_call, method_type, *method_declaration);
    if (recovering_from_error_) {
      return std::nullopt;
    }

    if (method_type.return_type == nullptr) {
      return std::nullopt;
    }

    return *method_type.return_type;
  }

  std::optional<Type> EvaluateFunctionCallType(const FunctionCall& function_call) {
    const UseResolver::ResolvedSymbol* resolved_symbol = ResolveUsedSymbol(
        function_call.function_name,
        &function_call,
        "Function call");
    if (recovering_from_error_) {
      return std::nullopt;
    }

    if (resolved_symbol->symbol_data.kind != SymbolKind::Function) {
      throw std::runtime_error("resolver is incorrect for this program");
    }

    const auto* function_type_ptr =
        std::get_if<FuncType>(&resolved_symbol->symbol_data.type.type);
    if (function_type_ptr == nullptr) {
      throw std::runtime_error(
          "symbol table is incorrect for this program");
    }

    const FuncType& function_type = *function_type_ptr;
    const FunctionDeclarationStatement* function_declaration =
        GetFunctionDeclaration(resolved_symbol->definition_node);
    TypeCheckCallArguments(
        function_call,
        function_type,
        *function_declaration);
    if (recovering_from_error_) {
      return std::nullopt;
    }

    if (function_type.return_type == nullptr) {
      return std::nullopt;
    }

    return *function_type.return_type;
  }

  const Program& program_;
  const UseResolver& use_resolver_;
  const TypeDefiner& type_definer_;
  DebugCtx& debug_ctx_;
  StatementNumerizer numerizer_;
  bool recovering_from_error_ = false;
  std::vector<std::optional<Type>> function_return_type_stack_;
  const ClassDeclarationStatement* current_method_owner_class_ = nullptr;
};

}  // namespace

void CheckTypes(
    const Program& program,
    const UseResolver& use_resolver,
    const TypeDefiner& type_definer,
    DebugCtx& debug_ctx) {
  TypeCheckerVisitor visitor(
      program,
      use_resolver,
      type_definer,
      debug_ctx);
  visitor.Check();
}

void CheckTypes(
    const Program& program,
    const UseResolver& use_resolver,
    const TypeDefiner& type_definer) {
  DebugCtx debug_ctx;
  CheckTypes(program, use_resolver, type_definer, debug_ctx);
  if (debug_ctx.GetErrors().HasErrors()) {
    debug_ctx.GetErrors().ThrowErrors();
  }
}

}  // namespace Parsing
