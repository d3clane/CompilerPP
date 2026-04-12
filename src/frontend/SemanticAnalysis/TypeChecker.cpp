#include "SemanticAnalysis/TypeChecker.hpp"

#include <cassert>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "Debug/DebugCtx.hpp"
#include "SemanticAnalysis/StatementNumerizer.hpp"
#include "Utils/ClassMemberLookup.hpp"
#include "Utils/Overload.hpp"

namespace Parsing {

namespace {

std::string TypeToString(const Type* type);

std::string TypeToString(const Type* type) {
  if (type == nullptr) {
    return "void";
  }

  return std::visit(
      Utils::Overload{
          [](const BoolType&) -> std::string {
            return "bool";
          },
          [](const IntType&) -> std::string {
            return "int";
          },
          [](const ClassType& class_type) -> std::string {
            if (class_type.parent == nullptr) {
              return "<invalid-class>";
            }

            return class_type.parent->class_name.name;
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

            result += " -> " + TypeToString(function_type.return_type);
            return result;
          }},
      type->type);
}

class TypeCheckerVisitor {
 public:
  TypeCheckerVisitor(
      const Program& program,
      const UseResolver& use_resolver,
      DebugCtx& debug_ctx)
      : program_(program),
        use_resolver_(use_resolver),
        debug_ctx_(debug_ctx),
        numerizer_(BuildStatementNumerizer(program)) {}

  void Check() {
    VisitStatements(program_.top_statements);
  }

 private:
  void ReportCodeError(const ASTNode* node, const std::string& message) {
    debug_ctx_.GetErrors().AddError(node, message);
    recovering_from_error_ = true;
  }

  const Type* GetIntType() const {
    return program_.type_storage.GetIntType();
  }

  const Type* GetBoolType() const {
    return program_.type_storage.GetBoolType();
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
  bool IsSupportedExprOp(const Type* type) const {
    if (type == nullptr) {
      return false;
    }

    return std::visit(
        []<typename ValueType>(const ValueType&) {
          return Parsing::IsSupportedExprOp<OpType, ValueType>();
        },
        type->type);
  }

  bool IsDerivedClassOf(
      const ClassType* derived_class,
      const ClassType* base_class) const {
    if (derived_class == nullptr || base_class == nullptr) {
      return false;
    }

    std::set<const ClassType*> visited_classes;
    const ClassType* current_class = derived_class;
    while (current_class != nullptr) {
      if (current_class == base_class) {
        return true;
      }

      if (!visited_classes.insert(current_class).second) {
        return false;
      }

      current_class = current_class->base_class;
    }

    return false;
  }

  bool IsTypeAssignable(const Type* expected_type, const Type* actual_type) const {
    if (expected_type == nullptr || actual_type == nullptr) {
      return expected_type == actual_type;
    }

    if (expected_type == actual_type) {
      return true;
    }

    const ClassType* expected_class_type = AsClassType(expected_type);
    const ClassType* actual_class_type = AsClassType(actual_type);
    if (expected_class_type == nullptr || actual_class_type == nullptr) {
      return false;
    }

    return IsDerivedClassOf(actual_class_type, expected_class_type);
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

    const ASTNode* declaration_scope_owner = declaration_ref->parent_node;
    if (declaration_scope_owner == nullptr) {
      throw std::runtime_error("statement numerizer is incorrect for this program");
    }

    // Forward class use is allowed in global scope.
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

  const Type* RequireValueType(
      const Expression& expression,
      const std::string& error_context) {
    const Type* evaluated_type = EvaluateExpressionType(expression);
    if (evaluated_type == nullptr) {
      if (!recovering_from_error_) {
        ReportCodeError(&expression, error_context + ": expression has no value");
      }
      return nullptr;
    }

    return evaluated_type;
  }

  void ValidateTypeReferences(
      const Type* type,
      const ASTNode* node,
      const std::string& error_context) {
    if (recovering_from_error_ || type == nullptr) {
      return;
    }

    std::visit(
        Utils::Overload{
            [](const IntType&) {},
            [](const BoolType&) {},
            [this, node, &error_context](const ClassType& class_type) {
              const ClassDeclarationStatement* class_declaration = class_type.parent;
              if (class_declaration == nullptr ||
                  !IsClassTypeVisible(*class_declaration, node)) {
                const std::string class_name =
                    class_type.parent != nullptr
                        ? class_type.parent->class_name.name
                        : "<invalid-class>";
                ReportCodeError(
                    node,
                    error_context + ": unknown class type " + class_name);
              }
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
                    function_type.return_type,
                    node,
                    error_context);
              }
            }},
        type->type);
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
            [this](const DeleteStatement& delete_statement) {
              VisitDeleteStatement(delete_statement);
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

    const Type* initializer_type =
        RequireValueType(*declaration.initializer, "Declaration initializer");
    if (recovering_from_error_ || initializer_type == nullptr) {
      return;
    }

    if (!IsTypeAssignable(declaration.type, initializer_type)) {
      ReportCodeError(
          &declaration,
          "Type mismatch in declaration of " + declaration.variable_name.name +
              ": expected " + TypeToString(declaration.type) +
              ", got " + TypeToString(initializer_type));
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

    if (function_declaration.GetReturnType() != nullptr) {
      ValidateTypeReferences(
          function_declaration.GetReturnType(),
          &function_declaration,
          "Function return type");
      if (recovering_from_error_) {
        return;
      }
    }

    function_return_type_stack_.push_back(function_declaration.GetReturnType());
    VisitStatements(function_declaration.body->statements);
    function_return_type_stack_.pop_back();
  }

  void VisitFunctionDeclarationStatement(
      const FunctionDeclarationStatement& function_declaration) {
    VisitFunctionDeclarationStatementBody(function_declaration);
  }

  void VisitClassDeclarationStatement(
      const ClassDeclarationStatement& class_declaration) {
    const ClassType* class_type = AsClassType(class_declaration.class_type);
    if (class_type != nullptr &&
        class_type->base_class != nullptr) {
      const ClassDeclarationStatement* base_class =
          class_type->base_class->parent;
      if (base_class == nullptr) {
        const std::string base_name =
            class_type->base_class->parent != nullptr
                ? class_type->base_class->parent->class_name.name
                : "<invalid-class>";
        ReportCodeError(
            &class_declaration,
            "Unknown base class: " + base_name);
      } else if (class_type != nullptr &&
                 IsDerivedClassOf(
                     AsClassType(base_class->class_type),
                     class_type)) {
        ReportCodeError(
            &class_declaration,
            "Cyclic class inheritance detected");
      }
    }

    for (size_t i = 0; i < class_declaration.fields.size(); ++i) {
      recovering_from_error_ = false;
      VisitDeclarationStatement(class_declaration.fields[i]);
    }

    for (size_t i = 0; i < class_declaration.methods.size(); ++i) {
      recovering_from_error_ = false;
      VisitFunctionDeclarationStatementBody(class_declaration.methods[i]);
    }
  }

  void VisitAssignmentStatement(const AssignmentStatement& assignment) {
    assert(assignment.expr != nullptr);

    const UseResolver::ResolvedSymbol* resolved_symbol = ResolveUsedSymbol(
        assignment.variable_name.name,
        &assignment,
        "Assignment");
    if (recovering_from_error_) {
      return;
    }

    const Type* value_type =
        RequireValueType(*assignment.expr, "Assignment");
    if (recovering_from_error_ || value_type == nullptr) {
      return;
    }

    if (!IsTypeAssignable(resolved_symbol->symbol_data.type, value_type)) {
      ReportCodeError(
          &assignment,
          "Type mismatch in assignment to " + assignment.variable_name.name +
              ": expected " + TypeToString(resolved_symbol->symbol_data.type) +
              ", got " + TypeToString(value_type));
    }
  }

  void VisitPrintStatement(const PrintStatement& print_statement) {
    assert(print_statement.expr != nullptr);
    RequireValueType(*print_statement.expr, "Print statement");
  }

  void VisitDeleteStatement(const DeleteStatement& delete_statement) {
    const UseResolver::ResolvedSymbol* resolved_symbol = ResolveUsedSymbol(
        delete_statement.variable.name,
        &delete_statement.variable,
        "Delete statement");
    if (recovering_from_error_) {
      return;
    }

    if (resolved_symbol->symbol_data.kind != SymbolKind::Variable &&
        resolved_symbol->symbol_data.kind != SymbolKind::Parameter) {
      ReportCodeError(
          &delete_statement.variable,
          "Delete target must be a variable: " + delete_statement.variable.name);
      return;
    }

    if (AsClassType(resolved_symbol->symbol_data.type) == nullptr) {
      ReportCodeError(
          &delete_statement.variable,
          "Delete target must have class type: " + delete_statement.variable.name);
    }
  }

  void VisitIfStatement(const IfStatement& if_statement) {
    assert(if_statement.condition != nullptr);
    assert(if_statement.true_block != nullptr);
    assert(if_statement.else_tail != nullptr);

    const Type* condition_type =
        RequireValueType(*if_statement.condition, "If condition");
    if (recovering_from_error_ || condition_type == nullptr) {
      return;
    }

    if (!IsBoolType(condition_type)) {
      ReportCodeError(
          &if_statement,
          "If condition must be bool, got " + TypeToString(condition_type));
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

    const Type* expected_type = function_return_type_stack_.back();
    if (expected_type == nullptr) {
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

    const Type* actual_type =
        RequireValueType(*return_statement.expr, "Return statement");
    if (recovering_from_error_ || actual_type == nullptr) {
      return;
    }

    if (!IsTypeAssignable(expected_type, actual_type)) {
      ReportCodeError(
          &return_statement,
          "Return type mismatch: expected " + TypeToString(expected_type) +
              ", got " + TypeToString(actual_type));
    }
  }

  void VisitExpressionStatement(const Expression& expression) {
    static_cast<void>(EvaluateExpressionType(expression));
  }

  void VisitBlock(const Block& block) {
    VisitStatements(block.statements);
  }

  template <UnaryExpressionNode Node>
  const Type* EvaluateUnaryExpressionType(const Node& unary_expression) {
    assert(unary_expression.operand != nullptr);
    const Type* operand_type =
        RequireValueType(*unary_expression.operand, "Unary expression");
    if (recovering_from_error_ || operand_type == nullptr) {
      return nullptr;
    }

    if (!IsSupportedExprOp<Node>(operand_type)) {
      ReportCodeError(
          &unary_expression,
          "Unary operation is not supported for type " + TypeToString(operand_type));
      return nullptr;
    }

    if constexpr (std::is_same_v<Node, UnaryNotExpression>) {
      return GetBoolType();
    }

    return operand_type;
  }

  template <BinaryExpressionNode Node>
  const Type* EvaluateBinaryExpressionType(const Node& binary_expression) {
    assert(binary_expression.left != nullptr);
    assert(binary_expression.right != nullptr);
    const Type* left_type =
        RequireValueType(*binary_expression.left, "Binary expression left operand");
    if (recovering_from_error_ || left_type == nullptr) {
      return nullptr;
    }

    const Type* right_type =
        RequireValueType(*binary_expression.right, "Binary expression right operand");
    if (recovering_from_error_ || right_type == nullptr) {
      return nullptr;
    }

    if (left_type != right_type) {
      ReportCodeError(
          &binary_expression,
          "Binary expression type mismatch: left is " + TypeToString(left_type) +
              ", right is " + TypeToString(right_type));
      return nullptr;
    }

    if (!IsSupportedExprOp<Node>(left_type)) {
      ReportCodeError(
          &binary_expression,
          "Binary operation is not supported for type " + TypeToString(left_type));
      return nullptr;
    }

    // Comparison and boolean ops always return bool.
    if constexpr (
        std::is_same_v<Node, LogicalAndExpression> ||
        std::is_same_v<Node, LogicalOrExpression> ||
        std::is_same_v<Node, EqualExpression> ||
        std::is_same_v<Node, NotEqualExpression> ||
        std::is_same_v<Node, LessExpression> ||
        std::is_same_v<Node, GreaterExpression> ||
        std::is_same_v<Node, LessEqualExpression> ||
        std::is_same_v<Node, GreaterEqualExpression>) {
      return GetBoolType();
    }

    return left_type;
  }

  const Type* EvaluateExpressionType(const Expression& expression) {
    if (recovering_from_error_) {
      return nullptr;
    }

    return std::visit(
        Utils::Overload{
            [this](const IdentifierExpression& identifier_expression)
                -> const Type* {
              const UseResolver::ResolvedSymbol* resolved_symbol = ResolveUsedSymbol(
                  identifier_expression.name,
                  &identifier_expression,
                  "Identifier");
              return resolved_symbol->symbol_data.type;
            },
            [this](const LiteralExpression& literal_expression) -> const Type* {
              return std::visit(
                  Utils::Overload{
                      [this](int) -> const Type* {
                        return GetIntType();
                      },
                      [this](bool) -> const Type* {
                        return GetBoolType();
                      }},
                  literal_expression.value);
            },
            [this](const FunctionCall& function_call) -> const Type* {
              return EvaluateFunctionCallType(function_call);
            },
            [this](const FieldAccess& field_access) -> const Type* {
              return EvaluateFieldAccessType(field_access);
            },
            [this](const MethodCall& method_call) -> const Type* {
              return EvaluateMethodCallType(method_call);
            },
            [this]<UnaryExpressionNode Node>(const Node& unary_expression)
                -> const Type* {
              return EvaluateUnaryExpressionType(unary_expression);
            },
            [this]<BinaryExpressionNode Node>(const Node& binary_expression)
                -> const Type* {
              return EvaluateBinaryExpressionType(binary_expression);
            }},
        expression.value);
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
          "Function call " + function_call.function_name.name +
              " mixes named and positional arguments");
      return;
    }

    size_t parameter_index = function_type.parameter_types.size();
    for (size_t i = 0; i < function_declaration.parameters.size(); ++i) {
      if (function_declaration.parameters[i].name.name == named_argument.name.name) {
        parameter_index = i;
        break;
      }
    }

    if (parameter_index == function_type.parameter_types.size()) {
      ReportCodeError(
          &named_argument,
          "Unknown named argument " + named_argument.name.name +
              " for function " + function_call.function_name.name);
      return;
    }

    if (!state.used_parameter_indices.insert(parameter_index).second) {
      ReportCodeError(
          &named_argument,
          "Duplicate named argument " + named_argument.name.name +
              " for function " + function_call.function_name.name);
      return;
    }

    assert(named_argument.value != nullptr);
    const Type* argument_type = RequireValueType(
        *named_argument.value,
        "Function named argument");
    if (recovering_from_error_ || argument_type == nullptr) {
      return;
    }

    const Type* expected_type = function_type.parameter_types[parameter_index];
    if (!IsTypeAssignable(expected_type, argument_type)) {
      ReportCodeError(
          &named_argument,
          "Type mismatch in named argument " + named_argument.name.name +
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
      ReportCodeError(
          &function_call,
          "Function call " + function_call.function_name.name +
              " mixes named and positional arguments");
      return;
    }

    if (state.positional_index >= function_type.parameter_types.size()) {
      ReportCodeError(
          &function_call,
          "Function " + function_call.function_name.name +
              " argument count mismatch");
      return;
    }

    assert(positional_argument.value != nullptr);
    const Type* argument_type = RequireValueType(
        *positional_argument.value,
        "Function positional argument");
    if (recovering_from_error_ || argument_type == nullptr) {
      return;
    }

    const Type* expected_type = function_type.parameter_types[state.positional_index];
    if (!IsTypeAssignable(expected_type, argument_type)) {
      ReportCodeError(
          &positional_argument,
          "Type mismatch in positional argument " +
              std::to_string(state.positional_index) +
              ": expected " + TypeToString(expected_type) +
              ", got " + TypeToString(argument_type));
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
          "Function " + function_call.function_name.name +
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

  const Type* EvaluateFieldAccessType(const FieldAccess& field_access) {
    const UseResolver::ResolvedSymbol* resolved_receiver = ResolveUsedSymbol(
        field_access.object_name.name,
        &field_access.object_name,
        "Field access receiver");
    if (recovering_from_error_) {
      return nullptr;
    }

    const ClassType* receiver_class_type = AsClassType(resolved_receiver->symbol_data.type);
    if (receiver_class_type == nullptr) {
      ReportCodeError(
          &field_access,
          "Field access receiver is not a class object: " + field_access.object_name.name);
      return nullptr;
    }

    const std::optional<ClassMemberLookupResult> resolved_field = LookupClassMember(
        *receiver_class_type,
        field_access.field_name.name,
        ClassMemberSearchMode::CurrentClassOnly);
    if (!resolved_field.has_value() ||
        resolved_field->kind != ClassMemberKind::Field ||
        resolved_field->field_declaration == nullptr) {
      const std::string class_name =
          receiver_class_type->parent != nullptr
              ? receiver_class_type->parent->class_name.name
              : "<invalid-class>";
      ReportCodeError(
          &field_access,
          "unknown field " + field_access.field_name.name +
              " for class " + class_name);
      return nullptr;
    }

    return resolved_field->field_declaration->type;
  }

  const Type* EvaluateMethodCallType(const MethodCall& method_call) {
    const UseResolver::ResolvedSymbol* resolved_receiver = ResolveUsedSymbol(
        method_call.object_name.name,
        &method_call.object_name,
        "Method call receiver");
    if (recovering_from_error_) {
      return nullptr;
    }

    const ClassType* receiver_class_type = AsClassType(resolved_receiver->symbol_data.type);
    if (receiver_class_type == nullptr) {
      ReportCodeError(
          &method_call,
          "Method call receiver is not a class object: " + method_call.object_name.name);
      return nullptr;
    }

    const std::optional<ClassMemberLookupResult> resolved_method = LookupClassMember(
        *receiver_class_type,
        method_call.function_call.function_name.name,
        ClassMemberSearchMode::CurrentClassAndBases);
    if (!resolved_method.has_value() ||
        resolved_method->kind != ClassMemberKind::Method ||
        resolved_method->method_declaration == nullptr) {
      const std::string class_name =
          receiver_class_type->parent != nullptr
              ? receiver_class_type->parent->class_name.name
              : "<invalid-class>";
      ReportCodeError(
          &method_call,
          "unknown method " + method_call.function_call.function_name.name +
              " for class " + class_name);
      return nullptr;
    }

    const FunctionDeclarationStatement* method_declaration =
        resolved_method->method_declaration;
    const FuncType* method_type = AsFuncType(method_declaration->function_type);
    if (method_type == nullptr) {
      throw std::runtime_error("invalid method declaration type");
    }

    TypeCheckCallArguments(method_call.function_call, *method_type, *method_declaration);
    if (recovering_from_error_) {
      return nullptr;
    }

    return method_type->return_type;
  }

  const Type* EvaluateFunctionCallType(const FunctionCall& function_call) {
    const UseResolver::ResolvedSymbol* resolved_symbol = ResolveUsedSymbol(
        function_call.function_name.name,
        &function_call,
        "Function call");
    if (recovering_from_error_) {
      return nullptr;
    }

    if (resolved_symbol->symbol_data.kind != SymbolKind::Function) {
      throw std::runtime_error("resolver is incorrect for this program");
    }

    const FuncType* function_type = AsFuncType(resolved_symbol->symbol_data.type);
    if (function_type == nullptr) {
      throw std::runtime_error(
          "symbol table is incorrect for this program");
    }

    const FunctionDeclarationStatement* function_declaration =
        GetFunctionDeclaration(resolved_symbol->definition_node);
    TypeCheckCallArguments(
        function_call,
        *function_type,
        *function_declaration);
    if (recovering_from_error_) {
      return nullptr;
    }

    return function_type->return_type;
  }

  const Program& program_;
  const UseResolver& use_resolver_;
  DebugCtx& debug_ctx_;
  StatementNumerizer numerizer_;
  bool recovering_from_error_ = false;
  std::vector<const Type*> function_return_type_stack_;
};

}  // namespace

void CheckTypes(
    const Program& program,
    const UseResolver& use_resolver,
    DebugCtx& debug_ctx) {
  TypeCheckerVisitor visitor(
      program,
      use_resolver,
      debug_ctx);
  visitor.Check();
}

void CheckTypes(
    const Program& program,
    const UseResolver& use_resolver) {
  DebugCtx debug_ctx;
  CheckTypes(program, use_resolver, debug_ctx);
  if (debug_ctx.GetErrors().HasErrors()) {
    debug_ctx.GetErrors().ThrowErrors();
  }
}

}  // namespace Parsing
