%option noyywrap
%option nodefault

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

#include "Debug/Debug.hpp"
#include "Debug/Errors.hpp"
#include "Tokenizing/Lexer.hpp"

namespace {

std::vector<Tokenizing::TokenVariant>* g_tokens = nullptr;
std::vector<Front::DebugInfo>* g_token_debug_infos = nullptr;
Front::FrontendErrors* g_errors = nullptr;

std::string g_filename;

size_t g_offset = 0;
size_t g_line = 1;
size_t g_column = 1;

struct Position {
  size_t offset;
  size_t line;
  size_t column;
};

Position CurrentPosition() {
  return Position{g_offset, g_line, g_column};
}

void AdvancePositionForChar(char ch) {
  ++g_offset;
  if (ch == '\n') {
    ++g_line;
    g_column = 1;
    return;
  }

  ++g_column;
}

void AdvancePosition(const char* text, int length) {
  for (int i = 0; i < length; ++i) {
    AdvancePositionForChar(text[i]);
  }
}

Front::DebugInfo BuildDebugInfo(
    const Position& start,
    const Position& end) {
  return Front::CreateDebugInfo(
      g_filename,
      {start.line, end.line},
      {start.column, end.column},
      {start.offset, end.offset});
}

template <typename T>
void PushToken(T token, const Position& start, const Position& end) {
  assert(g_tokens != nullptr);
  g_tokens->push_back(std::move(token));
  if (g_token_debug_infos != nullptr) {
    g_token_debug_infos->push_back(BuildDebugInfo(start, end));
  }
}

#define CREATE_TOKEN(NAME)                         \
  do {                                             \
    const Position start = CurrentPosition();      \
    AdvancePosition(yytext, yyleng);               \
    PushToken(Tokenizing::NAME{}, start, CurrentPosition()); \
  } while (false)

void AddLexError(const Position& start, const Position& end, const std::string& message) {
  if (g_errors == nullptr) {
    throw std::runtime_error(message);
  }

  g_errors->AddError(BuildDebugInfo(start, end), message);
}

bool TryParseIntegerLiteral(const char* text, int* value) {
  errno = 0;
  char* end = nullptr;
  const long parsed_value = std::strtol(text, &end, 10);

  if (errno == ERANGE || end == nullptr || *end != '\0') {
    return false;
  }

  *value = static_cast<int>(parsed_value);
  return true;
}

}  // namespace
%}

%x RECOVER

WHITESPACE [ \t\r\n]+
IDENT_START [A-Za-z_]
IDENT_BODY [A-Za-z0-9_]

%%
{WHITESPACE}    {
                  AdvancePosition(yytext, yyleng);
                }

"var"           { CREATE_TOKEN(VarKeyword); }
"int"           { CREATE_TOKEN(IntKeyword); }
"bool"          { CREATE_TOKEN(BoolKeyword); }
"func"          { CREATE_TOKEN(FuncKeyword); }
"class"         { CREATE_TOKEN(ClassKeyword); }
"return"        { CREATE_TOKEN(ReturnKeyword); }
"true"          { CREATE_TOKEN(TrueKeyword); }
"false"         { CREATE_TOKEN(FalseKeyword); }
"print"         { CREATE_TOKEN(PrintKeyword); }
"if"            { CREATE_TOKEN(IfKeyword); }
"else"          { CREATE_TOKEN(ElseKeyword); }
"delete"        { CREATE_TOKEN(DeleteKeyword); }

"=="            { CREATE_TOKEN(EqualEqual); }
"!="            { CREATE_TOKEN(NotEqual); }
"&&"            { CREATE_TOKEN(AndAnd); }
"||"            { CREATE_TOKEN(OrOr); }
"<="            { CREATE_TOKEN(LessEqual); }
">="            { CREATE_TOKEN(GreaterEqual); }
"<"             { CREATE_TOKEN(Less); }
">"             { CREATE_TOKEN(Greater); }
"="             { CREATE_TOKEN(Assign); }
"!"             { CREATE_TOKEN(Not); }
"+"             { CREATE_TOKEN(Plus); }
"-"             { CREATE_TOKEN(Minus); }
"*"             { CREATE_TOKEN(Star); }
"/"             { CREATE_TOKEN(Slash); }
"%"             { CREATE_TOKEN(Percent); }

";"             { CREATE_TOKEN(Semicolon); }
","             { CREATE_TOKEN(Comma); }
":"             { CREATE_TOKEN(Colon); }
"."             { CREATE_TOKEN(Dot); }
"("             { CREATE_TOKEN(LeftParen); }
")"             { CREATE_TOKEN(RightParen); }
"{"             { CREATE_TOKEN(LeftBrace); }
"}"             { CREATE_TOKEN(RightBrace); }

[0-9]+          {
                  const Position start = CurrentPosition();
                  AdvancePosition(yytext, yyleng);
                  const Position end = CurrentPosition();

                  int parsed_value = 0;
                  if (!TryParseIntegerLiteral(yytext, &parsed_value)) {
                    AddLexError(start, end, "Integer literal is out of range");
                    BEGIN(RECOVER);
                  } else {
                    PushToken(Tokenizing::Number{parsed_value}, start, end);
                  }
                }

{IDENT_START}{IDENT_BODY}* {
                  const Position start = CurrentPosition();
                  AdvancePosition(yytext, yyleng);
                  const Position end = CurrentPosition();

                  if (yyleng > 128) {
                    AddLexError(start, end, "Identifier is too long");
                    BEGIN(RECOVER);
                  } else {
                    PushToken(
                        Tokenizing::Identifier{std::string(yytext, yytext + yyleng)},
                        start,
                        end);
                  }
                }

.               {
                  const Position start = CurrentPosition();
                  AdvancePosition(yytext, yyleng);
                  const Position end = CurrentPosition();

                  const std::string unknown_token(yytext, yytext + yyleng);
                  AddLexError(
                      start,
                      end,
                      "Tokenizing error, unknown token: " + unknown_token);
                  BEGIN(RECOVER);
                }

<RECOVER>{WHITESPACE} { BEGIN(INITIAL); }
<RECOVER>.            { AdvancePosition(yytext, yyleng); }
%%

namespace Tokenizing {

std::vector<TokenVariant> Tokenize(
    const std::string& input,
    const std::string& filename,
    Front::FrontendErrors* errors,
    std::vector<Front::DebugInfo>* token_debug_infos) {
  std::vector<TokenVariant> tokens;
  g_tokens = &tokens;
  g_token_debug_infos = token_debug_infos;
  g_errors = errors;
  g_filename = filename;

  if (g_token_debug_infos != nullptr) {
    g_token_debug_infos->clear();
  }

  g_offset = 0;
  g_line = 1;
  g_column = 1;

  YY_BUFFER_STATE buffer = yy_scan_bytes(input.c_str(), static_cast<int>(input.size()));

  try {
    yylex();
  } catch (...) {
    yy_delete_buffer(buffer);
    yylex_destroy();
    g_tokens = nullptr;
    g_token_debug_infos = nullptr;
    g_errors = nullptr;
    g_filename.clear();
    throw;
  }

  yy_delete_buffer(buffer);
  yylex_destroy();
  g_tokens = nullptr;
  g_token_debug_infos = nullptr;
  g_errors = nullptr;
  g_filename.clear();

  return tokens;
}

}  // namespace Tokenizing

#undef CREATE_TOKEN
