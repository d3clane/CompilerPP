%require "3.8"
%language "c++"

%defines
%define api.namespace {Parsing}
%define api.parser.class {BisonParser}
%define api.value.type variant
%define api.token.constructor
%define parse.error verbose

%code requires {
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Parsing/Ast.hpp"
#include "Tokenizing/Tokens.hpp"

namespace Parsing {

struct ParserState {
  const std::vector<Tokenizing::TokenVariant>& tokens;
  size_t current_index;
};

}  // namespace Parsing
}

%code {
#include <stdexcept>
#include <utility>
#include <variant>

#include "Utils/Overload.hpp"

namespace Parsing {

BisonParser::symbol_type yylex(ParserState& state);

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
      NamedCallArgument{std::move(name), std::make_unique<Expression>(std::move(value))});
}

}  // namespace Parsing
}

%parse-param { ParserState& state }
%parse-param { Program& output }
%lex-param { ParserState& state }

%token VAR_KW "var"
%token INT_KW "int"
%token BOOL_KW "bool"
%token MUTABLE_KW "mutable"
%token FUNC_KW "func"
%token RETURN_KW "return"
%token TRUE_KW "true"
%token FALSE_KW "false"
%token PRINT_KW "print"
%token IF_KW "if"
%token ELSE_KW "else"

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

%type <Statement> stmt
%type <Statement> decl_stmt
%type <DeclarationStatement> var_decl
%type <FunctionDeclarationStatement> func_decl
%type <AssignmentStatement> assign_stmt
%type <PrintStatement> print_stmt
%type <IfStatement> if_stmt
%type <ReturnStatement> return_stmt
%type <Expression> expr_stmt
%type <Block> block_stmt

%type <ElseTail> else_tail
%type <Block> block

%type <bool> mutable_opt
%type <Type> type
%type <std::optional<Type>> return_type_opt
%type <std::vector<FunctionParameter>> param_list_opt
%type <std::vector<FunctionParameter>> param_list
%type <FunctionParameter> param

%type <FunctionCall> func_call
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
    stmt_list {
      output.top_statements = std::move($1);
    }
;

stmt_list:
    %empty {
      $$ = List<Statement>();
    }
  | stmt_list stmt {
      $1.push_back(std::make_unique<Statement>(std::move($2)));
      $$ = std::move($1);
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
;

var_decl:
    VAR_KW mutable_opt IDENT type init_opt SEMICOLON {
      $$ = DeclarationStatement{
          std::move($3),
          std::move($4),
          $2,
          std::move($5)};
    }
;

func_decl:
    FUNC_KW IDENT LEFT_PAREN param_list_opt RIGHT_PAREN return_type_opt block {
      $$ = FunctionDeclarationStatement{
          std::move($2),
          std::move($4),
          std::move($6),
          std::make_unique<Block>(std::move($7))};
    }
;

mutable_opt:
    %empty {
      $$ = false;
    }
  | MUTABLE_KW {
      $$ = true;
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
      $$ = FunctionParameter{std::move($1), std::move($2)};
    }
;

type:
    INT_KW {
      $$ = Type{IntType{}};
    }
  | BOOL_KW {
      $$ = Type{BoolType{}};
    }
;

return_type_opt:
    %empty {
      $$ = std::nullopt;
    }
  | type {
      $$ = std::optional<Type>{std::move($1)};
    }
;

assign_stmt:
    IDENT ASSIGN expr SEMICOLON {
      $$ = AssignmentStatement{
          std::move($1),
          std::make_unique<Expression>(std::move($3))};
    }
;

print_stmt:
    PRINT_KW LEFT_PAREN expr RIGHT_PAREN SEMICOLON {
      $$ = PrintStatement{
          std::make_unique<Expression>(std::move($3))};
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
      $$ = FunctionCall{std::move($1), std::move($3)};
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
  | LEFT_PAREN expr RIGHT_PAREN {
      $$ = std::move($2);
    }
;

%%

namespace Parsing {

BisonParser::symbol_type yylex(ParserState& state) {
  if (state.current_index >= state.tokens.size()) {
    return BisonParser::make_YYEOF();
  }

  const Tokenizing::TokenVariant& token = state.tokens[state.current_index];
  ++state.current_index;

  return std::visit(
      Utils::Overload{
          [](const Tokenizing::VarKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_VAR_KW();
          },
          [](const Tokenizing::IntKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_INT_KW();
          },
          [](const Tokenizing::BoolKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_BOOL_KW();
          },
          [](const Tokenizing::MutableKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_MUTABLE_KW();
          },
          [](const Tokenizing::FuncKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_FUNC_KW();
          },
          [](const Tokenizing::ReturnKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_RETURN_KW();
          },
          [](const Tokenizing::TrueKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_TRUE_KW();
          },
          [](const Tokenizing::FalseKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_FALSE_KW();
          },
          [](const Tokenizing::PrintKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_PRINT_KW();
          },
          [](const Tokenizing::IfKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_IF_KW();
          },
          [](const Tokenizing::ElseKeyword&) -> BisonParser::symbol_type {
            return BisonParser::make_ELSE_KW();
          },
          [](const Tokenizing::Assign&) -> BisonParser::symbol_type {
            return BisonParser::make_ASSIGN();
          },
          [](const Tokenizing::AndAnd&) -> BisonParser::symbol_type {
            return BisonParser::make_AND_AND();
          },
          [](const Tokenizing::OrOr&) -> BisonParser::symbol_type {
            return BisonParser::make_OR_OR();
          },
          [](const Tokenizing::Not&) -> BisonParser::symbol_type {
            return BisonParser::make_NOT();
          },
          [](const Tokenizing::Plus&) -> BisonParser::symbol_type {
            return BisonParser::make_PLUS();
          },
          [](const Tokenizing::Minus&) -> BisonParser::symbol_type {
            return BisonParser::make_MINUS();
          },
          [](const Tokenizing::Star&) -> BisonParser::symbol_type {
            return BisonParser::make_STAR();
          },
          [](const Tokenizing::Slash&) -> BisonParser::symbol_type {
            return BisonParser::make_SLASH();
          },
          [](const Tokenizing::Percent&) -> BisonParser::symbol_type {
            return BisonParser::make_PERCENT();
          },
          [](const Tokenizing::Semicolon&) -> BisonParser::symbol_type {
            return BisonParser::make_SEMICOLON();
          },
          [](const Tokenizing::Comma&) -> BisonParser::symbol_type {
            return BisonParser::make_COMMA();
          },
          [](const Tokenizing::Colon&) -> BisonParser::symbol_type {
            return BisonParser::make_COLON();
          },
          [](const Tokenizing::LeftParen&) -> BisonParser::symbol_type {
            return BisonParser::make_LEFT_PAREN();
          },
          [](const Tokenizing::RightParen&) -> BisonParser::symbol_type {
            return BisonParser::make_RIGHT_PAREN();
          },
          [](const Tokenizing::LeftBrace&) -> BisonParser::symbol_type {
            return BisonParser::make_LEFT_BRACE();
          },
          [](const Tokenizing::RightBrace&) -> BisonParser::symbol_type {
            return BisonParser::make_RIGHT_BRACE();
          },
          [](const Tokenizing::EqualEqual&) -> BisonParser::symbol_type {
            return BisonParser::make_EQUAL_EQUAL();
          },
          [](const Tokenizing::NotEqual&) -> BisonParser::symbol_type {
            return BisonParser::make_NOT_EQUAL();
          },
          [](const Tokenizing::Less&) -> BisonParser::symbol_type {
            return BisonParser::make_LESS();
          },
          [](const Tokenizing::Greater&) -> BisonParser::symbol_type {
            return BisonParser::make_GREATER();
          },
          [](const Tokenizing::LessEqual&) -> BisonParser::symbol_type {
            return BisonParser::make_LESS_EQUAL();
          },
          [](const Tokenizing::GreaterEqual&) -> BisonParser::symbol_type {
            return BisonParser::make_GREATER_EQUAL();
          },
          [](const Tokenizing::Identifier& value) -> BisonParser::symbol_type {
            return BisonParser::make_IDENT(value.name);
          },
          [](const Tokenizing::Number& value) -> BisonParser::symbol_type {
            return BisonParser::make_NUM(value.value);
          }},
      token);
}

void BisonParser::error(const std::string& message) {
  throw std::runtime_error("Parse error: " + message);
}

}  // namespace Parsing
