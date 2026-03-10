#pragma once

#include <iosfwd>
#include <map>
#include <string>

#include "Parsing/Ast.hpp"

namespace Parsing {

struct InterpreterContext {
  std::map<std::string, int> variables;
};

InterpreterContext Interpret(const Program& program, std::ostream& output);
InterpreterContext Interpret(const Program& program);

}  // namespace Parsing
