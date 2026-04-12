#pragma once

#include <string>
#include <variant>

namespace Tokenizing {

#define ADD_TOKEN(name) \
  struct name { \
    friend bool operator==(const name& left, const name& right) = default; \
  };

#define ADD_OPERATOR_EQUAL(type) \
  friend bool operator==(const type& left, const type& right) = default;

ADD_TOKEN(VarKeyword)
ADD_TOKEN(IntKeyword)
ADD_TOKEN(BoolKeyword)
ADD_TOKEN(FuncKeyword)
ADD_TOKEN(ClassKeyword)
ADD_TOKEN(ReturnKeyword)
ADD_TOKEN(TrueKeyword)
ADD_TOKEN(FalseKeyword)
ADD_TOKEN(PrintKeyword)
ADD_TOKEN(IfKeyword)
ADD_TOKEN(ElseKeyword)
ADD_TOKEN(DeleteKeyword)

ADD_TOKEN(Assign)
ADD_TOKEN(AndAnd)
ADD_TOKEN(OrOr)
ADD_TOKEN(Not)
ADD_TOKEN(Plus)
ADD_TOKEN(Minus)
ADD_TOKEN(Star)
ADD_TOKEN(Slash)
ADD_TOKEN(Percent)
ADD_TOKEN(Dot)
ADD_TOKEN(Semicolon)
ADD_TOKEN(Comma)
ADD_TOKEN(Colon)
ADD_TOKEN(LeftParen)
ADD_TOKEN(RightParen)
ADD_TOKEN(LeftBrace)
ADD_TOKEN(RightBrace)

ADD_TOKEN(EqualEqual)
ADD_TOKEN(NotEqual)
ADD_TOKEN(Less)
ADD_TOKEN(Greater)
ADD_TOKEN(LessEqual)
ADD_TOKEN(GreaterEqual)

struct Identifier {
  std::string name;

  ADD_OPERATOR_EQUAL(Identifier)
};

struct Number {
  int value;

  ADD_OPERATOR_EQUAL(Number)
};

using TokenVariant = std::variant<
    VarKeyword,
    IntKeyword,
    BoolKeyword,
    FuncKeyword,
    ClassKeyword,
    ReturnKeyword,
    TrueKeyword,
    FalseKeyword,
    PrintKeyword,
    IfKeyword,
    ElseKeyword,
    DeleteKeyword,
    Assign,
    AndAnd,
    OrOr,
    Not,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Dot,
    Semicolon,
    Comma,
    Colon,
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    EqualEqual,
    NotEqual,
    Less,
    Greater,
    LessEqual,
    GreaterEqual,
    Identifier,
    Number>;

#undef ADD_OPERATOR_EQUAL
#undef ADD_TOKEN

}  // namespace Tokenizing
