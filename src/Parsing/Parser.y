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
#include <string>
#include <vector>

#include "Parsing/Ast.hpp"
#include "Tokenizing/Tokens.hpp"

namespace Parsing {

struct ParserState {
  const std::vector<Tokenizing::TokenVariant>& tokens;
  int current_index;
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

TopStatementVariant StatementToTopStatement(StatementVariant statement) {
  return std::visit(
      Utils::Overload{
          [](AssignmentStatement value) -> TopStatementVariant {
            return TopStatementVariant{std::move(value)};
          },
          [](PrintStatement value) -> TopStatementVariant {
            return TopStatementVariant{std::move(value)};
          },
          [](IfStatement value) -> TopStatementVariant {
            return TopStatementVariant{std::move(value)};
          }},
      std::move(statement));
}

}  // namespace Parsing
}

%parse-param { ParserState& state }
%parse-param { Program& output }
%lex-param { ParserState& state }

%token VAR_KW "var"
%token INT_KW "int"
%token PRINT_KW "print"
%token IF_KW "if"
%token ELSE_KW "else"

%token ASSIGN "="
%token PLUS "+"
%token MINUS "-"
%token STAR "*"
%token SLASH "/"
%token PERCENT "%"
%token SEMICOLON ";"
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

%type <std::vector<std::unique_ptr<TopStatementVariant>>> top_stmt_list
%type <std::unique_ptr<TopStatementVariant>> top_stmt
%type <DeclarationStatement> decl_stmt
%type <StatementVariant> stmt
%type <AssignmentStatement> assign_stmt
%type <PrintStatement> print_stmt
%type <IfStatement> if_stmt
%type <ElseTail> else_tail
%type <Block> block
%type <std::vector<std::unique_ptr<StatementVariant>>> inner_stmt_list
%type <BoolExpression> bool_expr
%type <ComparisonOperatorVariant> comp_op
%type <Expression> expr
%type <Expression> add_expr
%type <Expression> mul_expr
%type <Expression> unary_expr
%type <Expression> final_expr

%start program

%%

program:
    top_stmt_list {
      output.top_statements = std::move($1);
    }
;

top_stmt_list:
    %empty {
      $$ = std::vector<std::unique_ptr<TopStatementVariant>>();
    }
  | top_stmt_list top_stmt {
      $1.push_back(std::move($2));
      $$ = std::move($1);
    }
;

top_stmt:
    decl_stmt {
      $$ = std::make_unique<TopStatementVariant>(std::move($1));
    }
  | stmt {
      $$ = std::make_unique<TopStatementVariant>(StatementToTopStatement(std::move($1)));
    }
;

decl_stmt:
    VAR_KW IDENT INT_KW SEMICOLON {
      $$ = DeclarationStatement{std::move($2), "int"};
    }
;

stmt:
    assign_stmt {
      $$ = StatementVariant{std::move($1)};
    }
  | print_stmt {
      $$ = StatementVariant{std::move($1)};
    }
  | if_stmt {
      $$ = StatementVariant{std::move($1)};
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
    IF_KW bool_expr block else_tail {
      $$ = IfStatement{
          std::make_unique<BoolExpression>(std::move($2)),
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

block:
    LEFT_BRACE inner_stmt_list RIGHT_BRACE {
      $$ = Block{std::move($2)};
    }
;

inner_stmt_list:
    %empty {
      $$ = std::vector<std::unique_ptr<StatementVariant>>();
    }
  | inner_stmt_list stmt {
      $1.push_back(std::make_unique<StatementVariant>(std::move($2)));
      $$ = std::move($1);
    }
;

bool_expr:
    expr comp_op expr {
      $$ = BoolExpression{
          std::make_unique<Expression>(std::move($1)),
          std::make_unique<ComparisonOperatorVariant>(std::move($2)),
          std::make_unique<Expression>(std::move($3))};
    }
;

comp_op:
    EQUAL_EQUAL {
      $$ = ComparisonOperatorVariant{EqualComparison{}};
    }
  | NOT_EQUAL {
      $$ = ComparisonOperatorVariant{NotEqualComparison{}};
    }
  | LESS {
      $$ = ComparisonOperatorVariant{LessComparison{}};
    }
  | GREATER {
      $$ = ComparisonOperatorVariant{GreaterComparison{}};
    }
  | LESS_EQUAL {
      $$ = ComparisonOperatorVariant{LessEqualComparison{}};
    }
  | GREATER_EQUAL {
      $$ = ComparisonOperatorVariant{GreaterEqualComparison{}};
    }
;

expr:
    add_expr {
      $$ = std::move($1);
    }
;

add_expr:
    mul_expr {
      $$ = std::move($1);
    }
  | add_expr PLUS mul_expr {
      $$ = Expression{AddExpression{
          std::make_unique<Expression>(std::move($1)),
          std::make_unique<Expression>(std::move($3))}};
    }
  | add_expr MINUS mul_expr {
      $$ = Expression{SubtractExpression{
          std::make_unique<Expression>(std::move($1)),
          std::make_unique<Expression>(std::move($3))}};
    }
;

mul_expr:
    unary_expr {
      $$ = std::move($1);
    }
  | mul_expr STAR unary_expr {
      $$ = Expression{MultiplyExpression{
          std::make_unique<Expression>(std::move($1)),
          std::make_unique<Expression>(std::move($3))}};
    }
  | mul_expr SLASH unary_expr {
      $$ = Expression{DivideExpression{
          std::make_unique<Expression>(std::move($1)),
          std::make_unique<Expression>(std::move($3))}};
    }
  | mul_expr PERCENT unary_expr {
      $$ = Expression{ModuloExpression{
          std::make_unique<Expression>(std::move($1)),
          std::make_unique<Expression>(std::move($3))}};
    }
;

unary_expr:
    final_expr {
      $$ = std::move($1);
    }
  | PLUS unary_expr {
      $$ = Expression{UnaryPlusExpression{
          std::make_unique<Expression>(std::move($2))}};
    }
  | MINUS unary_expr {
      $$ = Expression{UnaryMinusExpression{
          std::make_unique<Expression>(std::move($2))}};
    }
;

final_expr:
    IDENT {
      $$ = Expression{IdentifierExpression{std::move($1)}};
    }
  | NUM {
      $$ = Expression{NumberExpression{$1}};
    }
  | LEFT_PAREN expr RIGHT_PAREN {
      $$ = std::move($2);
    }
;

%%

namespace Parsing {

BisonParser::symbol_type yylex(ParserState& state) {
  if (state.current_index >= static_cast<int>(state.tokens.size())) {
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
