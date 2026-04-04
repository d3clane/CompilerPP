#include "Parsing/Parser.hpp"

#include <cassert>
#include <optional>
#include <variant>
#include <vector>

#include "BisonParser.hpp"
#include "Debug/Debug.hpp"
#include "Debug/DebugCtx.hpp"
#include "Utils/Overload.hpp"

namespace Parsing {

namespace {

class AstDebugInfoPropagator {
 public:
  explicit AstDebugInfoPropagator(ASTDebugInfo& ast_debug_info)
      : ast_debug_info_(ast_debug_info) {}

  void Propagate(const Program& program) {
    const DebugInfo* inherited_debug_info = ast_debug_info_.GetDebugInfo(&program);
    VisitProgram(program, inherited_debug_info);
  }

 private:
  const DebugInfo* EnsureNodeDebugInfo(
      const ASTNode& node,
      const DebugInfo* inherited_debug_info) {
    const DebugInfo* existing_debug_info = ast_debug_info_.GetDebugInfo(&node);
    if (existing_debug_info != nullptr) {
      return existing_debug_info;
    }

    if (inherited_debug_info == nullptr) {
      return nullptr;
    }

    ast_debug_info_.AddDebugInfo(&node, *inherited_debug_info);
    return ast_debug_info_.GetDebugInfo(&node);
  }

  void VisitProgram(const Program& program, const DebugInfo* inherited_debug_info) {
    const DebugInfo* current_debug_info =
        EnsureNodeDebugInfo(program, inherited_debug_info);
    VisitStatements(program.top_statements, current_debug_info);
  }

  void VisitStatements(
      const List<Statement>& statements,
      const DebugInfo* inherited_debug_info) {
    for (size_t i = 0; i < statements.size(); ++i) {
      assert(statements[i] != nullptr);
      VisitStatement(*statements[i], inherited_debug_info);
    }
  }

  void VisitStatement(
      const Statement& statement,
      const DebugInfo* inherited_debug_info) {
    const DebugInfo* current_debug_info =
        EnsureNodeDebugInfo(statement, inherited_debug_info);
    std::visit(
        Utils::Overload{
            [this, current_debug_info](const DeclarationStatement& declaration) {
              VisitDeclarationStatement(declaration, current_debug_info);
            },
            [this, current_debug_info](const FunctionDeclarationStatement& function_declaration) {
              VisitFunctionDeclaration(function_declaration, current_debug_info);
            },
            [this, current_debug_info](const ClassDeclarationStatement& class_declaration) {
              VisitClassDeclaration(class_declaration, current_debug_info);
            },
            [this, current_debug_info](const AssignmentStatement& assignment) {
              VisitAssignmentStatement(assignment, current_debug_info);
            },
            [this, current_debug_info](const PrintStatement& print_statement) {
              VisitPrintStatement(print_statement, current_debug_info);
            },
            [this, current_debug_info](const IfStatement& if_statement) {
              VisitIfStatement(if_statement, current_debug_info);
            },
            [this, current_debug_info](const ReturnStatement& return_statement) {
              VisitReturnStatement(return_statement, current_debug_info);
            },
            [this, current_debug_info](const Expression& expression) {
              VisitExpression(expression, current_debug_info);
            },
            [this, current_debug_info](const Block& block) {
              VisitBlock(block, current_debug_info);
            }},
        statement.value);
  }

  void VisitDeclarationStatement(
      const DeclarationStatement& declaration,
      const DebugInfo* inherited_debug_info) {
    const DebugInfo* current_debug_info =
        EnsureNodeDebugInfo(declaration, inherited_debug_info);
    if (declaration.initializer != nullptr) {
      VisitExpression(*declaration.initializer, current_debug_info);
    }
  }

  void VisitFunctionDeclaration(
      const FunctionDeclarationStatement& function_declaration,
      const DebugInfo* inherited_debug_info) {
    const DebugInfo* current_debug_info =
        EnsureNodeDebugInfo(function_declaration, inherited_debug_info);

    for (size_t i = 0; i < function_declaration.parameters.size(); ++i) {
      EnsureNodeDebugInfo(function_declaration.parameters[i], current_debug_info);
    }

    if (function_declaration.body != nullptr) {
      VisitBlock(*function_declaration.body, current_debug_info);
    }
  }

  void VisitClassDeclaration(
      const ClassDeclarationStatement& class_declaration,
      const DebugInfo* inherited_debug_info) {
    const DebugInfo* current_debug_info =
        EnsureNodeDebugInfo(class_declaration, inherited_debug_info);

    for (size_t i = 0; i < class_declaration.fields.size(); ++i) {
      VisitDeclarationStatement(class_declaration.fields[i], current_debug_info);
    }

    for (size_t i = 0; i < class_declaration.methods.size(); ++i) {
      VisitFunctionDeclaration(class_declaration.methods[i], current_debug_info);
    }
  }

  void VisitAssignmentStatement(
      const AssignmentStatement& assignment,
      const DebugInfo* inherited_debug_info) {
    const DebugInfo* current_debug_info =
        EnsureNodeDebugInfo(assignment, inherited_debug_info);
    if (assignment.expr != nullptr) {
      VisitExpression(*assignment.expr, current_debug_info);
    }
  }

  void VisitPrintStatement(
      const PrintStatement& print_statement,
      const DebugInfo* inherited_debug_info) {
    const DebugInfo* current_debug_info =
        EnsureNodeDebugInfo(print_statement, inherited_debug_info);
    if (print_statement.expr != nullptr) {
      VisitExpression(*print_statement.expr, current_debug_info);
    }
  }

  void VisitIfStatement(
      const IfStatement& if_statement,
      const DebugInfo* inherited_debug_info) {
    const DebugInfo* current_debug_info =
        EnsureNodeDebugInfo(if_statement, inherited_debug_info);
    if (if_statement.condition != nullptr) {
      VisitExpression(*if_statement.condition, current_debug_info);
    }
    if (if_statement.true_block != nullptr) {
      VisitBlock(*if_statement.true_block, current_debug_info);
    }
    if (if_statement.else_tail != nullptr) {
      VisitElseTail(*if_statement.else_tail, current_debug_info);
    }
  }

  void VisitElseTail(
      const ElseTail& else_tail,
      const DebugInfo* inherited_debug_info) {
    const DebugInfo* current_debug_info =
        EnsureNodeDebugInfo(else_tail, inherited_debug_info);
    if (else_tail.else_if != nullptr) {
      VisitIfStatement(*else_tail.else_if, current_debug_info);
      return;
    }

    if (else_tail.else_block != nullptr) {
      VisitBlock(*else_tail.else_block, current_debug_info);
    }
  }

  void VisitReturnStatement(
      const ReturnStatement& return_statement,
      const DebugInfo* inherited_debug_info) {
    const DebugInfo* current_debug_info =
        EnsureNodeDebugInfo(return_statement, inherited_debug_info);
    if (return_statement.expr != nullptr) {
      VisitExpression(*return_statement.expr, current_debug_info);
    }
  }

  void VisitBlock(const Block& block, const DebugInfo* inherited_debug_info) {
    const DebugInfo* current_debug_info =
        EnsureNodeDebugInfo(block, inherited_debug_info);
    VisitStatements(block.statements, current_debug_info);
  }

  void VisitExpression(
      const Expression& expression,
      const DebugInfo* inherited_debug_info) {
    const DebugInfo* current_debug_info =
        EnsureNodeDebugInfo(expression, inherited_debug_info);
    std::visit(
        Utils::Overload{
            [this, current_debug_info](const IdentifierExpression& identifier) {
              EnsureNodeDebugInfo(identifier, current_debug_info);
            },
            [this, current_debug_info](const LiteralExpression& literal) {
              EnsureNodeDebugInfo(literal, current_debug_info);
            },
            [this, current_debug_info](const auto& unary_expression)
              requires UnaryExpressionNode<decltype(unary_expression)> {
                VisitUnaryExpression(unary_expression, current_debug_info);
              },
            [this, current_debug_info](const auto& binary_expression)
              requires BinaryExpressionNode<decltype(binary_expression)> {
                VisitBinaryExpression(binary_expression, current_debug_info);
              },
            [this, current_debug_info](const FunctionCall& function_call) {
              VisitFunctionCall(function_call, current_debug_info);
            },
            [this, current_debug_info](const FieldAccess& field_access) {
              VisitFieldAccess(field_access, current_debug_info);
            },
            [this, current_debug_info](const MethodCall& method_call) {
              VisitMethodCall(method_call, current_debug_info);
            }},
        expression.value);
  }

  template <UnaryExpressionNode T>
  void VisitUnaryExpression(
      const T& unary_expression,
      const DebugInfo* inherited_debug_info) {
    const DebugInfo* current_debug_info =
        EnsureNodeDebugInfo(unary_expression, inherited_debug_info);
    assert(unary_expression.operand != nullptr);
    VisitExpression(*unary_expression.operand, current_debug_info);
  }

  template <BinaryExpressionNode T>
  void VisitBinaryExpression(
      const T& binary_expression,
      const DebugInfo* inherited_debug_info) {
    const DebugInfo* current_debug_info =
        EnsureNodeDebugInfo(binary_expression, inherited_debug_info);
    assert(binary_expression.left != nullptr);
    assert(binary_expression.right != nullptr);
    VisitExpression(*binary_expression.left, current_debug_info);
    VisitExpression(*binary_expression.right, current_debug_info);
  }

  void VisitFunctionCall(
      const FunctionCall& function_call,
      const DebugInfo* inherited_debug_info) {
    const DebugInfo* current_debug_info =
        EnsureNodeDebugInfo(function_call, inherited_debug_info);
    for (size_t i = 0; i < function_call.arguments.size(); ++i) {
      assert(function_call.arguments[i] != nullptr);
      VisitCallArgument(*function_call.arguments[i], current_debug_info);
    }
  }

  void VisitCallArgument(
      const CallArgument& argument,
      const DebugInfo* inherited_debug_info) {
    std::visit(
        Utils::Overload{
            [this, inherited_debug_info](const NamedCallArgument& named_argument) {
              const DebugInfo* current_debug_info =
                  EnsureNodeDebugInfo(named_argument, inherited_debug_info);
              assert(named_argument.value != nullptr);
              VisitExpression(*named_argument.value, current_debug_info);
            },
            [this, inherited_debug_info](const PositionalCallArgument& positional_argument) {
              const DebugInfo* current_debug_info =
                  EnsureNodeDebugInfo(positional_argument, inherited_debug_info);
              assert(positional_argument.value != nullptr);
              VisitExpression(*positional_argument.value, current_debug_info);
            }},
        argument);
  }

  void VisitFieldAccess(
      const FieldAccess& field_access,
      const DebugInfo* inherited_debug_info) {
    const DebugInfo* current_debug_info =
        EnsureNodeDebugInfo(field_access, inherited_debug_info);
    EnsureNodeDebugInfo(field_access.object_name, current_debug_info);
    EnsureNodeDebugInfo(field_access.field_name, current_debug_info);
  }

  void VisitMethodCall(
      const MethodCall& method_call,
      const DebugInfo* inherited_debug_info) {
    const DebugInfo* current_debug_info =
        EnsureNodeDebugInfo(method_call, inherited_debug_info);
    EnsureNodeDebugInfo(method_call.object_name, current_debug_info);
    VisitFunctionCall(method_call.function_call, current_debug_info);
  }

  ASTDebugInfo& ast_debug_info_;
};

Program ParseTokensImpl(
    const std::vector<Tokenizing::TokenVariant>& tokens,
    DebugCtx& debug_ctx,
    const std::vector<DebugInfo>* token_debug_infos,
    size_t input_size) {
  FrontendErrors& errors = debug_ctx.GetErrors();
  Program parsed_program;
  ParserState parser_state{
      tokens,
      0,
      token_debug_infos,
      &debug_ctx.GetAstDebugInfo(),
      &errors,
      debug_ctx.GetFilename(),
      input_size,
      std::vector<PendingErrorState>()};

  BisonParser parser(parser_state, parsed_program);
  const int parse_status = parser.parse();

  if (parse_status != 0) {
    errors.AddError(
        CreateDebugInfo(
            debug_ctx.GetFilename(),
            {0, 0},
            {0, 0},
            {0, input_size}),
        "Parsing failed");
  }

  AstDebugInfoPropagator(debug_ctx.GetAstDebugInfo()).Propagate(parsed_program);

  return parsed_program;
}

}  // namespace

Program ParseTokens(
    const std::vector<Tokenizing::TokenVariant>& tokens,
    DebugCtx& debug_ctx,
    const std::vector<DebugInfo>* token_debug_infos,
    size_t input_size) {
  return ParseTokensImpl(tokens, debug_ctx, token_debug_infos, input_size);
}

}  // namespace Parsing
