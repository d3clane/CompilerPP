#pragma once

#include <iosfwd>
#include <map>
#include <variant>

#include "Parsing/Ast.hpp"

namespace Parsing {

using RuntimeValue = std::variant<int, bool>;

struct InterpreterContext {
  std::map<const ASTNode*, RuntimeValue> variables;
};

InterpreterContext Interpret(const Program& program, std::ostream& output);
InterpreterContext Interpret(const Program& program);

}  // namespace Parsing
