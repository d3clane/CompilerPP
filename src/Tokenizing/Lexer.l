%option noyywrap
%option nodefault
%option nounput

%{
#include <cassert>
#include <cerrno>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Tokenizing/Lexer.hpp"

namespace {

std::vector<Tokenizing::TokenVariant>* g_tokens = nullptr;

template <typename T>
void PushToken(T token) {
  assert(g_tokens != nullptr);
  g_tokens->push_back(std::move(token));
}

int ParseIntegerLiteral(const char* text) {
  errno = 0;
  char* end = nullptr;
  const long value = std::strtol(text, &end, 10);

  if (errno == ERANGE || end == nullptr || *end != '\0') {
    throw std::runtime_error("Integer literal is out of range");
  }

  return static_cast<int>(value);
}

}  // namespace
%}

WHITESPACE [ \t\r\n]+
IDENT_START [A-Za-z_]
IDENT_BODY [A-Za-z0-9_]

%%
{WHITESPACE}    { }

"var"           { PushToken(Tokenizing::VarKeyword{}); }
"int"           { PushToken(Tokenizing::IntKeyword{}); }
"print"         { PushToken(Tokenizing::PrintKeyword{}); }
"if"            { PushToken(Tokenizing::IfKeyword{}); }
"else"          { PushToken(Tokenizing::ElseKeyword{}); }

"=="            { PushToken(Tokenizing::EqualEqual{}); }
"!="            { PushToken(Tokenizing::NotEqual{}); }
"<="            { PushToken(Tokenizing::LessEqual{}); }
">="            { PushToken(Tokenizing::GreaterEqual{}); }
"<"             { PushToken(Tokenizing::Less{}); }
">"             { PushToken(Tokenizing::Greater{}); }
"="             { PushToken(Tokenizing::Assign{}); }

";"             { PushToken(Tokenizing::Semicolon{}); }
"("             { PushToken(Tokenizing::LeftParen{}); }
")"             { PushToken(Tokenizing::RightParen{}); }
"{"             { PushToken(Tokenizing::LeftBrace{}); }
"}"             { PushToken(Tokenizing::RightBrace{}); }

-?[0-9]+        { PushToken(Tokenizing::Number{ParseIntegerLiteral(yytext)}); }

{IDENT_START}{IDENT_BODY}* {
                  if (yyleng > 128) {
                    throw std::runtime_error("Identifier is too long");
                  }
                  PushToken(Tokenizing::Identifier{std::string(yytext, yytext + yyleng)});
                }

.               {
                  std::string unknown_token(yytext, yytext + yyleng);
                  int next_char = yyinput();
                  while (next_char != '\0' &&
                         std::isspace(next_char) == 0) {
                    unknown_token.push_back(static_cast<char>(next_char));
                    next_char = yyinput();
                  }
                  throw std::runtime_error("Tokenizing error, unknown token: " + unknown_token);
                }
%%

namespace Tokenizing {

std::vector<TokenVariant> Tokenize(const std::string& input) {
  std::vector<TokenVariant> tokens;
  g_tokens = &tokens;

  const int input_length = static_cast<int>(input.size());
  YY_BUFFER_STATE buffer = yy_scan_bytes(input.c_str(), input_length);

  try {
    yylex();
  } catch (...) {
    yy_delete_buffer(buffer);
    yylex_destroy();
    g_tokens = nullptr;
    throw;
  }

  yy_delete_buffer(buffer);
  yylex_destroy();
  g_tokens = nullptr;

  return tokens;
}

}  // namespace Tokenizing
