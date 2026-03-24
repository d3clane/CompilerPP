#pragma once

#include <iosfwd>
#include <unordered_map>
#include <variant>

#include "Parsing/Ast.hpp"

namespace Parsing {

using RuntimeValue = std::variant<int, bool>;

struct InterpreterContext {
  std::unordered_map<AstNodeID, RuntimeValue> variables;
};

InterpreterContext Interpret(const Program& program, std::ostream& output);
InterpreterContext Interpret(const Program& program);

}  // namespace Parsing
