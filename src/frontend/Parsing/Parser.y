%require "3.8"
%language "c++"

%defines
%define api.namespace {Front}
%define api.parser.class {BisonParser}
%define api.value.type variant
%define api.token.constructor
%define parse.error detailed
%define api.location.type {Front::TokenLocation}
%locations

%code requires {
#include <cstddef>
#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Debug/Debug.hpp"
#include "Debug/Errors.hpp"
#include "Parsing/Ast.hpp"
#include "Tokenizing/Tokens.hpp"

namespace Front {

struct TokenLocation {
  size_t begin_token_idx = 0;
  size_t end_token_idx = 0;
  bool has_value = false;
};

struct PendingErrorState {
  std::string error_msg;
  size_t invalid_token_idx;
};

struct ParserState {
  const std::vector<Tokenizing::TokenVariant>& tokens;
  size_t current_index;
  const std::vector<DebugInfo>* token_debug_infos;
  ASTDebugInfo* ast_debug_info;
  FrontendErrors* errors;
  std::string filename;
  size_t input_size;

  std::vector<PendingErrorState> pending_errors;
};

}  // namespace Front

#define YYLLOC_DEFAULT(Current, Rhs, N)                                     \
  do {                                                                      \
    if ((N) != 0) {                                                         \
      (Current).begin_token_idx = YYRHSLOC(Rhs, 1).begin_token_idx;         \
      (Current).end_token_idx = YYRHSLOC(Rhs, N).end_token_idx;             \
      (Current).has_value =                                                  \
          YYRHSLOC(Rhs, 1).has_value && YYRHSLOC(Rhs, N).has_value;         \
    } else {                                                                \
      (Current).begin_token_idx = YYRHSLOC(Rhs, 0).end_token_idx;           \
      (Current).end_token_idx = YYRHSLOC(Rhs, 0).end_token_idx;             \
      (Current).has_value = false;                                          \
    }                                                                       \
  } while (false)
}

%code {
#include <algorithm>
#include <stdexcept>
#include <utility>
#include <variant>

#include "Utils/Overload.hpp"

namespace Front {

BisonParser::symbol_type yylex(ParserState& state);

DebugInfo BuildDebugInfoFromLocation(
    const ParserState& state,
    const TokenLocation& location) {
  if (!location.has_value ||
      state.token_debug_infos == nullptr ||
      state.token_debug_infos->empty()) {
    return CreateDebugInfo(
        state.filename,
        {0, 0},
        {0, 0},
        {0, state.input_size});
  }

  const size_t max_index = state.token_debug_infos->size() - 1;
  size_t begin_idx = std::min(location.begin_token_idx, max_index);
  size_t end_idx = std::min(location.end_token_idx, max_index);
  if (begin_idx > end_idx) {
    std::swap(begin_idx, end_idx);
  }

  const DebugInfo& begin_token_info = (*state.token_debug_infos)[begin_idx];
  const DebugInfo& end_token_info = (*state.token_debug_infos)[end_idx];

  size_t code_begin = std::min(begin_token_info.code_range.first, state.input_size);
  size_t code_end = std::min(end_token_info.code_range.second, state.input_size);
  if (code_end < code_begin) {
    code_end = code_begin;
  }

  DebugInfo debug_info = CreateDebugInfo(
      state.filename,
      {begin_token_info.line_range.first, end_token_info.line_range.second},
      {begin_token_info.column_range.first, end_token_info.column_range.second},
      {code_begin, code_end});
  if (code_end > code_begin) {
    debug_info.stressed_chars.push_back({0, code_end - code_begin});
  }
  return debug_info;
}

void AddNodeDebugInfo(
    ParserState& state,
    const ASTNode* node,
    const TokenLocation& location) {
  if (state.ast_debug_info == nullptr || node == nullptr) {
    return;
  }

  state.ast_debug_info->AddDebugInfo(
      node,
      BuildDebugInfoFromLocation(state, location));
}

template <typename T>
Expression MakeBinaryExpression(Expression left, Expression right) {
  return Expression{ExpressionVariant{T{
      std::make_unique<Expression>(std::move(left)),
      std::make_unique<Expression>(std::move(right))}}};
}

template <typename T>
Expression MakeUnaryExpression(Expression operand) {
  return Expression{ExpressionVariant{T{
      std::make_unique<Expression>(std::move(operand))}}};
}

std::unique_ptr<CallArgument> MakePositionalArgument(Expression value) {
  return std::make_unique<CallArgument>(
      PositionalCallArgument{std::make_unique<Expression>(std::move(value))});
}

std::unique_ptr<CallArgument> MakeNamedArgument(
    std::string name,
    Expression value) {
  return std::make_unique<CallArgument>(
      NamedCallArgument{
          IdentifierExpression{std::move(name)},
          std::make_unique<Expression>(std::move(value))});
}

std::vector<const Type*> CollectParameterTypes(
    const std::vector<FunctionParameter>& parameters) {
  std::vector<const Type*> parameter_types;
  parameter_types.reserve(parameters.size());
  for (const FunctionParameter& parameter : parameters) {
    parameter_types.push_back(parameter.type);
  }

  return parameter_types;
}

void SyncClassTypeWithDeclaration(ClassDeclarationStatement& class_declaration) {
  ClassType* class_type =
      const_cast<ClassType*>(AsClassType(class_declaration.class_type));
  if (class_type == nullptr) {
    return;
  }

  class_type->class_decl = &class_declaration;
  class_type->fields.clear();
  class_type->methods.clear();

  for (DeclarationStatement& field : class_declaration.fields) {
    class_type->fields.push_back(ClassFieldType{
        .type = field.type,
        .name = &field.variable_name});
  }

  for (FunctionDeclarationStatement& method : class_declaration.methods) {
    class_type->methods.push_back(ClassMethodType{
        .type = AsFuncType(method.function_type),
        .name = &method.function_name});
  }
}

void SyncClassTypeWithStatement(Statement& statement) {
  if (ClassDeclarationStatement* class_declaration =
          std::get_if<ClassDeclarationStatement>(&statement.value);
      class_declaration != nullptr) {
    SyncClassTypeWithDeclaration(*class_declaration);
  }
}

bool IsSemicolonToken(const Tokenizing::TokenVariant& token) {
  return std::holds_alternative<Tokenizing::Semicolon>(token);
}

bool IsLeftBraceToken(const Tokenizing::TokenVariant& token) {
  return std::holds_alternative<Tokenizing::LeftBrace>(token);
}

bool IsRightBraceToken(const Tokenizing::TokenVariant& token) {
  return std::holds_alternative<Tokenizing::RightBrace>(token);
}

size_t GetTokenRangeStart(const ParserState& state, size_t token_index) {
  if (state.token_debug_infos == nullptr || token_index >= state.token_debug_infos->size()) {
    return 0;
  }

  return (*state.token_debug_infos)[token_index].code_range.first;
}

size_t GetTokenRangeEnd(const ParserState& state, size_t token_index) {
  if (state.token_debug_infos == nullptr || token_index >= state.token_debug_infos->size()) {
    return 0;
  }

  return (*state.token_debug_infos)[token_index].code_range.second;
}

size_t FindPreviousBoundaryEnd(const ParserState& state, size_t from_index) {
  size_t index = std::min(from_index, state.tokens.size());
  while (index > 0) {
    --index;
    if (IsSemicolonToken(state.tokens[index]) || IsRightBraceToken(state.tokens[index])) {
      return GetTokenRangeEnd(state, index);
    }
  }

  return 0;
}

size_t FindNextTokenIndex(
    const ParserState& state,
    size_t from_index,
    bool (*predicate)(const Tokenizing::TokenVariant&)) {
  for (size_t index = from_index; index < state.tokens.size(); ++index) {
    if (predicate(state.tokens[index])) {
      return index;
    }
  }

  return state.tokens.size();
}

PendingErrorState GetPendingError(ParserState& state) {
  assert(!state.pending_errors.empty());

  const PendingErrorState pending_err = state.pending_errors.back();
  state.pending_errors.pop_back();

  return pending_err;
}

void AddPendingParseErrorWithContext(
    ParserState& state,
    const size_t error_token_index,
    const std::string& errors_message,
    size_t context_begin,
    size_t context_end) {
  if (state.errors == nullptr) {
    return;
  }

  size_t highlight_begin = GetTokenRangeStart(state, error_token_index);
  size_t highlight_end = GetTokenRangeEnd(state, error_token_index);

  context_begin = std::min(context_begin, state.input_size);
  context_end = std::min(context_end, state.input_size);
  if (context_end < context_begin) {
    context_end = context_begin;
  }

  highlight_begin = std::min(highlight_begin, state.input_size);
  highlight_end = std::min(highlight_end, state.input_size);
  if (highlight_end < highlight_begin) {
    highlight_end = highlight_begin;
  }

  DebugInfo debug_info = CreateDebugInfo(
      state.filename,
      {0, 0},
      {0, 0},
      {context_begin, context_end});

  if (state.token_debug_infos != nullptr &&
      error_token_index < state.token_debug_infos->size()) {
    debug_info.line_range = (*state.token_debug_infos)[error_token_index].line_range;
    debug_info.column_range = (*state.token_debug_infos)[error_token_index].column_range;
  }

  if (context_end > context_begin && highlight_end > highlight_begin) {
    const size_t stress_begin = highlight_begin > context_begin
                                    ? highlight_begin - context_begin
                                    : 0;
    const size_t stress_end = std::min(highlight_end, context_end) - context_begin;
    if (stress_end > stress_begin) {
      debug_info.stressed_chars.push_back({stress_begin, stress_end});
    }
  }

  state.errors->AddError(debug_info, errors_message);
}

void AddPendingParseErrorUntilSemicolon(ParserState& state) {
  const auto [message, error_token_index] = GetPendingError(state);
  const size_t context_begin = FindPreviousBoundaryEnd(state, error_token_index);
  const size_t semicolon_index = FindNextTokenIndex(
      state,
      error_token_index,
      IsSemicolonToken);
  const size_t context_end = semicolon_index < state.tokens.size()
                                 ? GetTokenRangeEnd(state, semicolon_index)
                                 : state.input_size;
  AddPendingParseErrorWithContext(state, error_token_index, message, 
                                  context_begin, context_end);
}

void AddPendingParseErrorUntilBlockBegin(ParserState& state) {
  const auto [message, error_token_index] = GetPendingError(state);
  const size_t context_begin = FindPreviousBoundaryEnd(state, error_token_index);
  const size_t block_begin_index = FindNextTokenIndex(
      state,
      error_token_index,
      IsLeftBraceToken);
  const size_t context_end = block_begin_index < state.tokens.size()
                                 ? GetTokenRangeStart(state, block_begin_index)
                                 : state.input_size;
  AddPendingParseErrorWithContext(state, error_token_index, message, 
                                  context_begin, context_end);
}

}  // namespace Front
}

%parse-param { ParserState& state }
%parse-param { Program& output }
%lex-param { ParserState& state }

%token VAR_KW "var"
%token INT_KW "int"
%token BOOL_KW "bool"
%token FUNC_KW "func"
%token CLASS_KW "class"
%token RETURN_KW "return"
%token TRUE_KW "true"
%token FALSE_KW "false"
%token PRINT_KW "print"
%token IF_KW "if"
%token ELSE_KW "else"
%token DELETE_KW "delete"

%token ASSIGN "="
%token AND_AND "&&"
%token OR_OR "||"
%token NOT "!"
%token PLUS "+"
%token MINUS "-"
%token STAR "*"
%token SLASH "/"
%token PERCENT "%"
%token SEMICOLON ";"
%token COMMA ","
%token COLON ":"
%token DOT "."
%token LEFT_PAREN "("
%token RIGHT_PAREN ")"
%token LEFT_BRACE "{"
%token RIGHT_BRACE "}"

%token EQUAL_EQUAL "=="
%token NOT_EQUAL "!="
%token LESS "<"
%token GREATER ">"
%token LESS_EQUAL "<="
%token GREATER_EQUAL ">="

%token <std::string> IDENT
%token <int> NUM

%type <List<Statement>> stmt_list
%type <List<Statement>> decl_stmt_list

%type <Statement> stmt
%type <Statement> decl_stmt
%type <DeclarationStatement> var_decl
%type <FunctionDeclarationStatement> func_decl
%type <ClassDeclarationStatement> class_decl
%type <AssignmentStatement> assign_stmt
%type <PrintStatement> print_stmt
%type <DeleteStatement> delete_stmt
%type <IfStatement> if_stmt
%type <ReturnStatement> return_stmt
%type <Expression> expr_stmt
%type <Block> block_stmt

%type <ElseTail> else_tail
%type <Block> block

%type <const Type*> type
%type <const Type*> base_type
%type <const Type*> return_type_opt
%type <std::optional<IdentifierExpression>> inheritance_opt
%type <std::vector<FunctionParameter>> param_list_opt
%type <std::vector<FunctionParameter>> param_list
%type <FunctionParameter> param
%type <std::vector<DeclarationStatement>> class_field_decl_list
%type <std::vector<FunctionDeclarationStatement>> class_method_decl_list

%type <FunctionCall> func_call
%type <FieldAccess> field_access
%type <MethodCall> method_call
%type <List<CallArgument>> call_args_opt
%type <List<CallArgument>> call_args
%type <List<CallArgument>> named_arg_list
%type <List<CallArgument>> pos_arg_list
%type <std::unique_ptr<CallArgument>> named_arg

%type <std::unique_ptr<Expression>> init_opt
%type <std::unique_ptr<Expression>> return_arg_opt

%type <Expression> expr
%type <Expression> logic_or_expr
%type <Expression> logic_and_expr
%type <Expression> equality_expr
%type <Expression> relational_expr
%type <Expression> add_expr
%type <Expression> mul_expr
%type <Expression> unary_expr
%type <Expression> final_expr

%start program

%%

program:
    decl_stmt_list {
      output.top_statements = std::move($1);
      AddNodeDebugInfo(state, &output, @$);
    }
;

decl_stmt_list:
    %empty {
      $$ = List<Statement>();
    }
  | decl_stmt_list decl_stmt {
      auto statement = std::make_unique<Statement>(std::move($2));
      SyncClassTypeWithStatement(*statement);
      AddNodeDebugInfo(state, statement.get(), @2);
      $1.push_back(std::move(statement));
      $$ = std::move($1);
    }
  | decl_stmt_list error SEMICOLON {
      AddPendingParseErrorUntilSemicolon(state);
      $$ = std::move($1);
      yyerrok;
    }
;

stmt_list:
    %empty {
      $$ = List<Statement>();
    }
  | stmt_list stmt {
      auto statement = std::make_unique<Statement>(std::move($2));
      SyncClassTypeWithStatement(*statement);
      AddNodeDebugInfo(state, statement.get(), @2);
      $1.push_back(std::move(statement));
      $$ = std::move($1);
    }
  | stmt_list error SEMICOLON {
      AddPendingParseErrorUntilSemicolon(state);
      $$ = std::move($1);
      yyerrok;
    }
;

stmt:
    decl_stmt {
      $$ = std::move($1);
    }
  | assign_stmt {
      $$ = Statement{StatementVariant{std::move($1)}};
    }
  | print_stmt {
      $$ = Statement{StatementVariant{std::move($1)}};
    }
  | delete_stmt {
      $$ = Statement{StatementVariant{std::move($1)}};
    }
  | if_stmt {
      $$ = Statement{StatementVariant{std::move($1)}};
    }
  | return_stmt {
      $$ = Statement{StatementVariant{std::move($1)}};
    }
  | expr_stmt {
      $$ = Statement{StatementVariant{std::move($1)}};
    }
  | block_stmt {
      $$ = Statement{StatementVariant{std::move($1)}};
    }
;

decl_stmt:
    var_decl {
      $$ = Statement{StatementVariant{std::move($1)}};
    }
  | func_decl {
      $$ = Statement{StatementVariant{std::move($1)}};
    }
  | class_decl {
      $$ = Statement{StatementVariant{std::move($1)}};
    }
;

var_decl:
    VAR_KW IDENT type init_opt SEMICOLON {
      $$ = DeclarationStatement{
          IdentifierExpression{std::move($2)},
          $3,
          std::move($4)};
    }
;

func_decl:
    FUNC_KW IDENT LEFT_PAREN param_list_opt RIGHT_PAREN return_type_opt block {
      const Type* function_type = output.type_storage.CreateFunctionType(
          $6,
          CollectParameterTypes($4));
      $$ = FunctionDeclarationStatement{
          function_type,
          IdentifierExpression{std::move($2)},
          std::move($4),
          std::make_unique<Block>(std::move($7))};
    }
  | FUNC_KW error block {
      AddPendingParseErrorUntilBlockBegin(state);
      $$ = FunctionDeclarationStatement{};
      yyerrok;
    }
;

class_decl:
    CLASS_KW IDENT inheritance_opt LEFT_BRACE class_field_decl_list class_method_decl_list RIGHT_BRACE {
      const Type* class_type_handle =
          output.type_storage.GetOrCreateClassType($2);
      ClassType* class_type =
          const_cast<ClassType*>(AsClassType(class_type_handle));
      assert(class_type != nullptr);
      const ClassType* base_class = nullptr;
      if ($3.has_value()) {
        const Type* base_class_type =
            output.type_storage.GetOrCreateClassType($3->name);
        base_class = AsClassType(base_class_type);
      }
      class_type->base_class = base_class;

      $$ = ClassDeclarationStatement{
          class_type_handle,
          IdentifierExpression{std::move($2)},
          std::move($5),
          std::move($6)};
    }
;

inheritance_opt:
    %empty {
      $$ = std::nullopt;
    }
  | COLON IDENT {
      $$ = std::optional<IdentifierExpression>{
          IdentifierExpression{std::move($2)}};
    }
;

class_field_decl_list:
    %empty {
      $$ = std::vector<DeclarationStatement>();
    }
  | class_field_decl_list var_decl {
      $1.push_back(std::move($2));
      $$ = std::move($1);
    }
;

class_method_decl_list:
    %empty {
      $$ = std::vector<FunctionDeclarationStatement>();
    }
  | class_method_decl_list func_decl {
      $1.push_back(std::move($2));
      $$ = std::move($1);
    }
;

init_opt:
    %empty {
      $$ = nullptr;
    }
  | ASSIGN expr {
      $$ = std::make_unique<Expression>(std::move($2));
    }
;

param_list_opt:
    %empty {
      $$ = std::vector<FunctionParameter>();
    }
  | param_list {
      $$ = std::move($1);
    }
;

param_list:
    param {
      $$ = std::vector<FunctionParameter>();
      $$.push_back(std::move($1));
    }
  | param_list COMMA param {
      $1.push_back(std::move($3));
      $$ = std::move($1);
    }
;

param:
    IDENT type {
      $$ = FunctionParameter{
          IdentifierExpression{std::move($1)},
          $2};
    }
;

type:
    base_type {
      $$ = $1;
    }
;

base_type:
    INT_KW {
      $$ = output.type_storage.GetIntType();
    }
  | BOOL_KW {
      $$ = output.type_storage.GetBoolType();
    }
  | IDENT {
      $$ = output.type_storage.GetOrCreateClassType($1);
    }
;

return_type_opt:
    %empty {
      $$ = nullptr;
    }
  | type {
      $$ = $1;
    }
;

assign_stmt:
    IDENT ASSIGN expr SEMICOLON {
      $$ = AssignmentStatement{
          IdentifierExpression{std::move($1)},
          std::make_unique<Expression>(std::move($3))};
    }
;

print_stmt:
    PRINT_KW LEFT_PAREN expr RIGHT_PAREN SEMICOLON {
      $$ = PrintStatement{
          std::make_unique<Expression>(std::move($3))};
    }
;

delete_stmt:
    DELETE_KW IDENT SEMICOLON {
      $$ = DeleteStatement{IdentifierExpression{std::move($2)}};
    }
;

if_stmt:
    IF_KW expr block else_tail {
      $$ = IfStatement{
          std::make_unique<Expression>(std::move($2)),
          std::make_unique<Block>(std::move($3)),
          std::make_unique<ElseTail>(std::move($4))};
    }
;

else_tail:
    %empty {
      $$ = ElseTail{
          nullptr,
          nullptr};
    }
  | ELSE_KW block {
      $$ = ElseTail{
          nullptr,
          std::make_unique<Block>(std::move($2))};
    }
  | ELSE_KW if_stmt {
      $$ = ElseTail{
          std::make_unique<IfStatement>(std::move($2)),
          nullptr};
    }
;

return_stmt:
    RETURN_KW return_arg_opt SEMICOLON {
      $$ = ReturnStatement{
          std::move($2)};
    }
;

return_arg_opt:
    %empty {
      $$ = nullptr;
    }
  | expr {
      $$ = std::make_unique<Expression>(std::move($1));
    }
;

expr_stmt:
    expr SEMICOLON {
      $$ = std::move($1);
    }
;

block_stmt:
    block {
      $$ = std::move($1);
    }
;

block:
    LEFT_BRACE stmt_list RIGHT_BRACE {
      $$ = Block{std::move($2)};
    }
;

func_call:
    IDENT LEFT_PAREN call_args_opt RIGHT_PAREN {
      $$ = FunctionCall{
          IdentifierExpression{std::move($1)},
          std::move($3)};
    }
;

field_access:
    IDENT DOT IDENT {
      $$ = FieldAccess{
          IdentifierExpression{std::move($1)},
          IdentifierExpression{std::move($3)}};
    }
;

method_call:
    IDENT DOT func_call {
      $$ = MethodCall{
          IdentifierExpression{std::move($1)},
          std::move($3)};
    }
;

call_args_opt:
    %empty {
      $$ = List<CallArgument>();
    }
  | call_args {
      $$ = std::move($1);
    }
;

call_args:
    named_arg_list {
      $$ = std::move($1);
    }
  | pos_arg_list {
      $$ = std::move($1);
    }
;

named_arg_list:
    named_arg {
      $$ = List<CallArgument>();
      $$.push_back(std::move($1));
    }
  | named_arg_list COMMA named_arg {
      $1.push_back(std::move($3));
      $$ = std::move($1);
    }
;

named_arg:
    IDENT COLON expr {
      $$ = MakeNamedArgument(std::move($1), std::move($3));
    }
;

pos_arg_list:
    expr {
      $$ = List<CallArgument>();
      $$.push_back(MakePositionalArgument(std::move($1)));
    }
  | pos_arg_list COMMA expr {
      $1.push_back(MakePositionalArgument(std::move($3)));
      $$ = std::move($1);
    }
;

expr:
    logic_or_expr {
      $$ = std::move($1);
    }
;

logic_or_expr:
    logic_and_expr {
      $$ = std::move($1);
    }
  | logic_or_expr OR_OR logic_and_expr {
      $$ = MakeBinaryExpression<LogicalOrExpression>(std::move($1), std::move($3));
    }
;

logic_and_expr:
    equality_expr {
      $$ = std::move($1);
    }
  | logic_and_expr AND_AND equality_expr {
      $$ = MakeBinaryExpression<LogicalAndExpression>(std::move($1), std::move($3));
    }
;

equality_expr:
    relational_expr {
      $$ = std::move($1);
    }
  | equality_expr EQUAL_EQUAL relational_expr {
      $$ = MakeBinaryExpression<EqualExpression>(std::move($1), std::move($3));
    }
  | equality_expr NOT_EQUAL relational_expr {
      $$ = MakeBinaryExpression<NotEqualExpression>(std::move($1), std::move($3));
    }
;

relational_expr:
    add_expr {
      $$ = std::move($1);
    }
  | relational_expr LESS add_expr {
      $$ = MakeBinaryExpression<LessExpression>(std::move($1), std::move($3));
    }
  | relational_expr GREATER add_expr {
      $$ = MakeBinaryExpression<GreaterExpression>(std::move($1), std::move($3));
    }
  | relational_expr LESS_EQUAL add_expr {
      $$ = MakeBinaryExpression<LessEqualExpression>(std::move($1), std::move($3));
    }
  | relational_expr GREATER_EQUAL add_expr {
      $$ = MakeBinaryExpression<GreaterEqualExpression>(std::move($1), std::move($3));
    }
;

add_expr:
    mul_expr {
      $$ = std::move($1);
    }
  | add_expr PLUS mul_expr {
      $$ = MakeBinaryExpression<AddExpression>(std::move($1), std::move($3));
    }
  | add_expr MINUS mul_expr {
      $$ = MakeBinaryExpression<SubtractExpression>(std::move($1), std::move($3));
    }
;

mul_expr:
    unary_expr {
      $$ = std::move($1);
    }
  | mul_expr STAR unary_expr {
      $$ = MakeBinaryExpression<MultiplyExpression>(std::move($1), std::move($3));
    }
  | mul_expr SLASH unary_expr {
      $$ = MakeBinaryExpression<DivideExpression>(std::move($1), std::move($3));
    }
  | mul_expr PERCENT unary_expr {
      $$ = MakeBinaryExpression<ModuloExpression>(std::move($1), std::move($3));
    }
;

unary_expr:
    final_expr {
      $$ = std::move($1);
    }
  | PLUS unary_expr {
      $$ = MakeUnaryExpression<UnaryPlusExpression>(std::move($2));
    }
  | MINUS unary_expr {
      $$ = MakeUnaryExpression<UnaryMinusExpression>(std::move($2));
    }
  | NOT unary_expr {
      $$ = MakeUnaryExpression<UnaryNotExpression>(std::move($2));
    }
;

final_expr:
    IDENT {
      $$ = Expression{ExpressionVariant{IdentifierExpression{std::move($1)}}};
    }
  | NUM {
      $$ = Expression{ExpressionVariant{LiteralExpression{LiteralValue{$1}}}};
    }
  | TRUE_KW {
      $$ = Expression{ExpressionVariant{LiteralExpression{LiteralValue{true}}}};
    }
  | FALSE_KW {
      $$ = Expression{ExpressionVariant{LiteralExpression{LiteralValue{false}}}};
    }
  | func_call {
      $$ = Expression{ExpressionVariant{std::move($1)}};
    }
  | field_access {
      $$ = Expression{ExpressionVariant{std::move($1)}};
    }
  | method_call {
      $$ = Expression{ExpressionVariant{std::move($1)}};
    }
  | LEFT_PAREN expr RIGHT_PAREN {
      $$ = std::move($2);
    }
;

%%

namespace Front {

BisonParser::symbol_type yylex(ParserState& state) {
  if (state.current_index >= state.tokens.size()) {
    return BisonParser::make_YYEOF(TokenLocation{0, 0, false});
  }

  const size_t token_index = state.current_index;
  const Tokenizing::TokenVariant& token = state.tokens[state.current_index];
  ++state.current_index;
  const TokenLocation location{token_index, token_index, true};

  return std::visit(
      Utils::Overload{
          [&location](const Tokenizing::VarKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_VAR_KW(location);
          },
          [&location](const Tokenizing::IntKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_INT_KW(location);
          },
          [&location](const Tokenizing::BoolKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_BOOL_KW(location);
          },
          [&location](const Tokenizing::FuncKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_FUNC_KW(location);
          },
          [&location](const Tokenizing::ClassKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_CLASS_KW(location);
          },
          [&location](const Tokenizing::ReturnKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_RETURN_KW(location);
          },
          [&location](const Tokenizing::TrueKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_TRUE_KW(location);
          },
          [&location](const Tokenizing::FalseKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_FALSE_KW(location);
          },
          [&location](const Tokenizing::PrintKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_PRINT_KW(location);
          },
          [&location](const Tokenizing::IfKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_IF_KW(location);
          },
          [&location](const Tokenizing::ElseKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_ELSE_KW(location);
          },
          [&location](const Tokenizing::DeleteKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_DELETE_KW(location);
          },
          [&location](const Tokenizing::Assign&) -> BisonParser::symbol_type {
            return BisonParser::make_ASSIGN(location);
          },
          [&location](const Tokenizing::AndAnd&) -> BisonParser::symbol_type {
            return BisonParser::make_AND_AND(location);
          },
          [&location](const Tokenizing::OrOr&) -> BisonParser::symbol_type {
            return BisonParser::make_OR_OR(location);
          },
          [&location](const Tokenizing::Not&) -> BisonParser::symbol_type {
            return BisonParser::make_NOT(location);
          },
          [&location](const Tokenizing::Plus&) -> BisonParser::symbol_type {
            return BisonParser::make_PLUS(location);
          },
          [&location](const Tokenizing::Minus&) -> BisonParser::symbol_type {
            return BisonParser::make_MINUS(location);
          },
          [&location](const Tokenizing::Star&) -> BisonParser::symbol_type {
            return BisonParser::make_STAR(location);
          },
          [&location](const Tokenizing::Slash&) -> BisonParser::symbol_type {
            return BisonParser::make_SLASH(location);
          },
          [&location](const Tokenizing::Percent&) -> BisonParser::symbol_type {
            return BisonParser::make_PERCENT(location);
          },
          [&location](const Tokenizing::Semicolon&) -> BisonParser::symbol_type {
            return BisonParser::make_SEMICOLON(location);
          },
          [&location](const Tokenizing::Comma&) -> BisonParser::symbol_type {
            return BisonParser::make_COMMA(location);
          },
          [&location](const Tokenizing::Colon&) -> BisonParser::symbol_type {
            return BisonParser::make_COLON(location);
          },
          [&location](const Tokenizing::Dot&) -> BisonParser::symbol_type {
            return BisonParser::make_DOT(location);
          },
          [&location](const Tokenizing::LeftParen&) -> BisonParser::symbol_type {
            return BisonParser::make_LEFT_PAREN(location);
          },
          [&location](const Tokenizing::RightParen&) -> BisonParser::symbol_type {
            return BisonParser::make_RIGHT_PAREN(location);
          },
          [&location](const Tokenizing::LeftBrace&) -> BisonParser::symbol_type {
            return BisonParser::make_LEFT_BRACE(location);
          },
          [&location](const Tokenizing::RightBrace&) -> BisonParser::symbol_type {
            return BisonParser::make_RIGHT_BRACE(location);
          },
          [&location](const Tokenizing::EqualEqual&) -> BisonParser::symbol_type {
            return BisonParser::make_EQUAL_EQUAL(location);
          },
          [&location](const Tokenizing::NotEqual&) -> BisonParser::symbol_type {
            return BisonParser::make_NOT_EQUAL(location);
          },
          [&location](const Tokenizing::Less&) -> BisonParser::symbol_type {
            return BisonParser::make_LESS(location);
          },
          [&location](const Tokenizing::Greater&) -> BisonParser::symbol_type {
            return BisonParser::make_GREATER(location);
          },
          [&location](const Tokenizing::LessEqual&) -> BisonParser::symbol_type {
            return BisonParser::make_LESS_EQUAL(location);
          },
          [&location](const Tokenizing::GreaterEqual&) -> BisonParser::symbol_type {
            return BisonParser::make_GREATER_EQUAL(location);
          },
          [&location](const Tokenizing::Identifier& value) -> BisonParser::symbol_type {
            return BisonParser::make_IDENT(value.name, location);
          },
          [&location](const Tokenizing::Number& value) -> BisonParser::symbol_type {
            return BisonParser::make_NUM(value.value, location);
          }},
      token);
}

void BisonParser::error(
    const location_type& location,
    const std::string& message) {
  if (state.errors == nullptr) {
    throw std::runtime_error("Parse error: " + message);
  }

  size_t invalid_token_idx = state.current_index == 0 ? 0 : state.current_index - 1;
  if (location.has_value) {
    invalid_token_idx = location.begin_token_idx;
  }

  state.pending_errors.push_back(PendingErrorState{
    "Parse error: " + message,
    invalid_token_idx
  });
}

}  // namespace Front
