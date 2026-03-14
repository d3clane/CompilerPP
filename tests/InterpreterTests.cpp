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

TEST(InterpreterTests, ExecutesElseIfBranch) {
  const std::string source =
      "var x int;\n"
      "x = 5;\n"
      "if x < 0 { print(0); } else if x < 10 { print(1); x = 7; } else { print(2); }\n"
      "print(x);\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);

  EXPECT_EQ(output.str(), "1\n7\n");
  ASSERT_EQ(context.variables.count("x"), 1);
  EXPECT_EQ(context.variables.at("x"), 7);
}

TEST(InterpreterTests, ExecutesMultipleElseIfBranches) {
  const std::string source =
      "var x int;\n"
      "x = 15;\n"
      "if x < 0 { print(0); } else if x < 10 { print(1); } else if x < 20 { print(2); x = 9; } else { print(3); }\n"
      "print(x);\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);

  EXPECT_EQ(output.str(), "2\n9\n");
  ASSERT_EQ(context.variables.count("x"), 1);
  EXPECT_EQ(context.variables.at("x"), 9);
}

TEST(InterpreterTests, ExecutesIfWithoutElseAndSkipsBodyWhenFalse) {
  const std::string source =
      "var x int;\n"
      "x = 3;\n"
      "if x < 0 { print(999); x = 1; }\n"
      "print(x);\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);

  EXPECT_EQ(output.str(), "3\n");
  ASSERT_EQ(context.variables.count("x"), 1);
  EXPECT_EQ(context.variables.at("x"), 3);
}

TEST(InterpreterTests, ExecutesIfWithoutElseWhenConditionIsTrue) {
  const std::string source =
      "var x int;\n"
      "x = 3;\n"
      "if x > 0 { print(7); x = 8; }\n"
      "print(x);\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);

  EXPECT_EQ(output.str(), "7\n8\n");
  ASSERT_EQ(context.variables.count("x"), 1);
  EXPECT_EQ(context.variables.at("x"), 8);
}

TEST(InterpreterTests, EvaluatesArithmeticExpressionsWithPrecedence) {
  const std::string source =
      "var x int;\n"
      "var y int;\n"
      "x = 10;\n"
      "y = 3;\n"
      "print(x + y * 2);\n"
      "print((x + y) * 2);\n"
      "x = -x + y % 2;\n"
      "if x + 4 == y / 1 { print(111); } else { print(x); }\n"
      "if -x / 3 == y { print(222); } else { print(x); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);

  EXPECT_EQ(output.str(), "16\n26\n-9\n222\n");
  ASSERT_EQ(context.variables.count("x"), 1);
  EXPECT_EQ(context.variables.at("x"), -9);
  ASSERT_EQ(context.variables.count("y"), 1);
  EXPECT_EQ(context.variables.at("y"), 3);
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

TEST(InterpreterTests, ThrowsOnDivisionByZero) {
  const std::string source =
      "var x int;\n"
      "x = 4 / (2 - 2);\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  EXPECT_THROW(Parsing::Interpret(program, output), std::runtime_error);
}

}  // namespace
