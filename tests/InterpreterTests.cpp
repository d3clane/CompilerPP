#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "Parsing/Interpreter.hpp"
#include "Parsing/Parser.hpp"

namespace {

TEST(InterpreterTests, ExecutesTrueBranchAndPrintsValues) {
  const std::string source =
      "var x int;\n"
      "x = 5;\n"
      "if x > 3 { print(x); x = 10; } else { print(0); }\n"
      "print(x);\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);

  EXPECT_EQ(output.str(), "5\n10\n");
  ASSERT_EQ(context.variables.count("x"), 1);
  EXPECT_EQ(context.variables.at("x"), 10);
}

TEST(InterpreterTests, ExecutesFalseBranchAndKeepsOrder) {
  const std::string source =
      "var x int;\n"
      "x = -1;\n"
      "if x >= 0 { print(1); } else { print(x); x = 2; }\n"
      "print(x);\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);

  EXPECT_EQ(output.str(), "-1\n2\n");
  ASSERT_EQ(context.variables.count("x"), 1);
  EXPECT_EQ(context.variables.at("x"), 2);
}

TEST(InterpreterTests, ThrowsOnPrintOfUnknownVariable) {
  const std::string source = "print(x);\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  EXPECT_THROW(Parsing::Interpret(program, output), std::runtime_error);
}

TEST(InterpreterTests, ThrowsOnAssignmentToUndeclaredVariable) {
  const std::string source = "x = 1;\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  EXPECT_THROW(Parsing::Interpret(program, output), std::runtime_error);
}

}  // namespace
