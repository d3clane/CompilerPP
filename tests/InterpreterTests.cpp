#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "Parsing/Interpreter.hpp"
#include "Parsing/Parser.hpp"

namespace {

TEST(InterpreterTests, ExecutesIntAndBoolBranches) {
  const std::string source =
      "var x int;\n"
      "var flag bool = true;\n"
      "x = 5;\n"
      "if flag && x > 3 { print(x); flag = false; } else { print(0); }\n"
      "print(flag);\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);

  EXPECT_EQ(output.str(), "5\nfalse\n");
  ASSERT_EQ(context.variables.count("x"), 1);
  const auto* x_value = std::get_if<int>(&context.variables.at("x"));
  ASSERT_NE(x_value, nullptr);
  EXPECT_EQ(*x_value, 5);
  ASSERT_EQ(context.variables.count("flag"), 1);
  const auto* flag_value = std::get_if<bool>(&context.variables.at("flag"));
  ASSERT_NE(flag_value, nullptr);
  EXPECT_FALSE(*flag_value);
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
  const auto* x_value = std::get_if<int>(&context.variables.at("x"));
  ASSERT_NE(x_value, nullptr);
  EXPECT_EQ(*x_value, 2);
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
  const auto* x_value = std::get_if<int>(&context.variables.at("x"));
  ASSERT_NE(x_value, nullptr);
  EXPECT_EQ(*x_value, 7);
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
  const auto* x_value = std::get_if<int>(&context.variables.at("x"));
  ASSERT_NE(x_value, nullptr);
  EXPECT_EQ(*x_value, 9);
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
  const auto* x_value = std::get_if<int>(&context.variables.at("x"));
  ASSERT_NE(x_value, nullptr);
  EXPECT_EQ(*x_value, 3);
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
  const auto* x_value = std::get_if<int>(&context.variables.at("x"));
  ASSERT_NE(x_value, nullptr);
  EXPECT_EQ(*x_value, 8);
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
  const auto* x_value = std::get_if<int>(&context.variables.at("x"));
  ASSERT_NE(x_value, nullptr);
  EXPECT_EQ(*x_value, -9);
  ASSERT_EQ(context.variables.count("y"), 1);
  const auto* y_value = std::get_if<int>(&context.variables.at("y"));
  ASSERT_NE(y_value, nullptr);
  EXPECT_EQ(*y_value, 3);
}

TEST(InterpreterTests, EvaluatesBooleanLogic) {
  const std::string source =
      "var a bool = true;\n"
      "var b bool = false;\n"
      "if a && !b || false { print(true); } else { print(false); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);

  EXPECT_EQ(output.str(), "true\n");
  ASSERT_EQ(context.variables.count("a"), 1);
  const auto* a_value = std::get_if<bool>(&context.variables.at("a"));
  ASSERT_NE(a_value, nullptr);
  EXPECT_TRUE(*a_value);
  ASSERT_EQ(context.variables.count("b"), 1);
  const auto* b_value = std::get_if<bool>(&context.variables.at("b"));
  ASSERT_NE(b_value, nullptr);
  EXPECT_FALSE(*b_value);
}

TEST(InterpreterTests, SupportsBoolAssignmentFromIdentifier) {
  const std::string source =
      "var a bool = true;\n"
      "var b bool;\n"
      "b = a;\n"
      "print(b);\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);

  EXPECT_EQ(output.str(), "true\n");
  ASSERT_EQ(context.variables.count("a"), 1);
  const auto* a_value = std::get_if<bool>(&context.variables.at("a"));
  ASSERT_NE(a_value, nullptr);
  EXPECT_TRUE(*a_value);
  ASSERT_EQ(context.variables.count("b"), 1);
  const auto* b_value = std::get_if<bool>(&context.variables.at("b"));
  ASSERT_NE(b_value, nullptr);
  EXPECT_TRUE(*b_value);
}

TEST(InterpreterTests, ThrowsOnFunctionCallStatement) {
  const std::string source = "foo();\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  EXPECT_THROW(Parsing::Interpret(program, output), std::runtime_error);
}

TEST(InterpreterTests, ThrowsOnFunctionCallInsideArithmeticExpression) {
  const std::string source =
      "var x int;\n"
      "x = foo() + 1;\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  EXPECT_THROW(Parsing::Interpret(program, output), std::runtime_error);
}

TEST(InterpreterTests, ThrowsOnFunctionCallInsideBoolInitializer) {
  const std::string source =
      "var flag bool = foo();\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  EXPECT_THROW(Parsing::Interpret(program, output), std::runtime_error);
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

TEST(InterpreterTests, DivisionLeftRecursionTest) {
  const std::string source = "print(100 / 5 / 2);\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  Parsing::Interpret(program, output);
  EXPECT_EQ(output.str(), "10\n");
}

TEST(InterpreterTests, SubtractionLeftRecursionTest) {
  const std::string source = "print(100 - 5 - 2);\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  Parsing::Interpret(program, output);
  EXPECT_EQ(output.str(), "93\n");
}

}  // namespace
