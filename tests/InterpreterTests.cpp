#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "Visitors/Interpreter.hpp"
#include "TestParseUtils.hpp"

namespace {

const Parsing::DeclarationStatement* GetTopDeclaration(
    const Parsing::Program& program,
    size_t index) {
  if (index >= program.top_statements.size()) {
    return nullptr;
  }

  if (program.top_statements[index] == nullptr) {
    return nullptr;
  }

  return std::get_if<Parsing::DeclarationStatement>(&program.top_statements[index]->value);
}

TEST(InterpreterTests, ExecutesIntAndBoolBranches) {
  const std::string source =
      "var x int;\n"
      "var flag bool = true;\n"
      "func main() { x = 5; if flag && x > 3 { print(x); flag = false; } else { print(0); } print(flag); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);
  const Parsing::DeclarationStatement* x_declaration = GetTopDeclaration(program, 0);
  const Parsing::DeclarationStatement* flag_declaration = GetTopDeclaration(program, 1);

  EXPECT_EQ(output.str(), "5\nfalse\n");
  ASSERT_NE(x_declaration, nullptr);
  ASSERT_NE(flag_declaration, nullptr);
  ASSERT_EQ(context.variables.count(x_declaration), 1);
  const auto* x_value = std::get_if<int>(&context.variables.at(x_declaration));
  ASSERT_NE(x_value, nullptr);
  EXPECT_EQ(*x_value, 5);
  ASSERT_EQ(context.variables.count(flag_declaration), 1);
  const auto* flag_value = std::get_if<bool>(&context.variables.at(flag_declaration));
  ASSERT_NE(flag_value, nullptr);
  EXPECT_FALSE(*flag_value);
}

TEST(InterpreterTests, ExecutesFalseBranchAndKeepsOrder) {
  const std::string source =
      "var x int;\n"
      "func main() { x = -1; if x >= 0 { print(1); } else { print(x); x = 2; } print(x); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);
  const Parsing::DeclarationStatement* x_declaration = GetTopDeclaration(program, 0);

  EXPECT_EQ(output.str(), "-1\n2\n");
  ASSERT_NE(x_declaration, nullptr);
  ASSERT_EQ(context.variables.count(x_declaration), 1);
  const auto* x_value = std::get_if<int>(&context.variables.at(x_declaration));
  ASSERT_NE(x_value, nullptr);
  EXPECT_EQ(*x_value, 2);
}

TEST(InterpreterTests, ExecutesElseIfBranch) {
  const std::string source =
      "var x int;\n"
      "func main() { x = 5; if x < 0 { print(0); } else if x < 10 { print(1); x = 7; } else { print(2); } print(x); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);
  const Parsing::DeclarationStatement* x_declaration = GetTopDeclaration(program, 0);

  EXPECT_EQ(output.str(), "1\n7\n");
  ASSERT_NE(x_declaration, nullptr);
  ASSERT_EQ(context.variables.count(x_declaration), 1);
  const auto* x_value = std::get_if<int>(&context.variables.at(x_declaration));
  ASSERT_NE(x_value, nullptr);
  EXPECT_EQ(*x_value, 7);
}

TEST(InterpreterTests, ExecutesMultipleElseIfBranches) {
  const std::string source =
      "var x int;\n"
      "func main() { x = 15; if x < 0 { print(0); } else if x < 10 { print(1); } else if x < 20 { print(2); x = 9; } else { print(3); } print(x); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);
  const Parsing::DeclarationStatement* x_declaration = GetTopDeclaration(program, 0);

  EXPECT_EQ(output.str(), "2\n9\n");
  ASSERT_NE(x_declaration, nullptr);
  ASSERT_EQ(context.variables.count(x_declaration), 1);
  const auto* x_value = std::get_if<int>(&context.variables.at(x_declaration));
  ASSERT_NE(x_value, nullptr);
  EXPECT_EQ(*x_value, 9);
}

TEST(InterpreterTests, ExecutesIfWithoutElseAndSkipsBodyWhenFalse) {
  const std::string source =
      "var x int;\n"
      "func main() { x = 3; if x < 0 { print(999); x = 1; } print(x); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);
  const Parsing::DeclarationStatement* x_declaration = GetTopDeclaration(program, 0);

  EXPECT_EQ(output.str(), "3\n");
  ASSERT_NE(x_declaration, nullptr);
  ASSERT_EQ(context.variables.count(x_declaration), 1);
  const auto* x_value = std::get_if<int>(&context.variables.at(x_declaration));
  ASSERT_NE(x_value, nullptr);
  EXPECT_EQ(*x_value, 3);
}

TEST(InterpreterTests, ExecutesIfWithoutElseWhenConditionIsTrue) {
  const std::string source =
      "var x int;\n"
      "func main() { x = 3; if x > 0 { print(7); x = 8; } print(x); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);
  const Parsing::DeclarationStatement* x_declaration = GetTopDeclaration(program, 0);

  EXPECT_EQ(output.str(), "7\n8\n");
  ASSERT_NE(x_declaration, nullptr);
  ASSERT_EQ(context.variables.count(x_declaration), 1);
  const auto* x_value = std::get_if<int>(&context.variables.at(x_declaration));
  ASSERT_NE(x_value, nullptr);
  EXPECT_EQ(*x_value, 8);
}

TEST(InterpreterTests, EvaluatesArithmeticExpressionsWithPrecedence) {
  const std::string source =
      "var x int;\n"
      "var y int;\n"
      "func main() { x = 10; y = 3; print(x + y * 2); print((x + y) * 2); x = -x + y % 2; if x + 4 == y / 1 { print(111); } else { print(x); } if -x / 3 == y { print(222); } else { print(x); } }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);
  const Parsing::DeclarationStatement* x_declaration = GetTopDeclaration(program, 0);
  const Parsing::DeclarationStatement* y_declaration = GetTopDeclaration(program, 1);

  EXPECT_EQ(output.str(), "16\n26\n-9\n222\n");
  ASSERT_NE(x_declaration, nullptr);
  ASSERT_NE(y_declaration, nullptr);
  ASSERT_EQ(context.variables.count(x_declaration), 1);
  const auto* x_value = std::get_if<int>(&context.variables.at(x_declaration));
  ASSERT_NE(x_value, nullptr);
  EXPECT_EQ(*x_value, -9);
  ASSERT_EQ(context.variables.count(y_declaration), 1);
  const auto* y_value = std::get_if<int>(&context.variables.at(y_declaration));
  ASSERT_NE(y_value, nullptr);
  EXPECT_EQ(*y_value, 3);
}

TEST(InterpreterTests, EvaluatesBooleanLogic) {
  const std::string source =
      "var a bool = true;\n"
      "var b bool = false;\n"
      "func main() { if a && !b || false { print(true); } else { print(false); } }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);
  const Parsing::DeclarationStatement* a_declaration = GetTopDeclaration(program, 0);
  const Parsing::DeclarationStatement* b_declaration = GetTopDeclaration(program, 1);

  EXPECT_EQ(output.str(), "true\n");
  ASSERT_NE(a_declaration, nullptr);
  ASSERT_NE(b_declaration, nullptr);
  ASSERT_EQ(context.variables.count(a_declaration), 1);
  const auto* a_value = std::get_if<bool>(&context.variables.at(a_declaration));
  ASSERT_NE(a_value, nullptr);
  EXPECT_TRUE(*a_value);
  ASSERT_EQ(context.variables.count(b_declaration), 1);
  const auto* b_value = std::get_if<bool>(&context.variables.at(b_declaration));
  ASSERT_NE(b_value, nullptr);
  EXPECT_FALSE(*b_value);
}

TEST(InterpreterTests, SupportsBoolAssignmentFromIdentifier) {
  const std::string source =
      "var a bool = true;\n"
      "var b bool;\n"
      "func main() { b = a; print(b); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);
  const Parsing::DeclarationStatement* a_declaration = GetTopDeclaration(program, 0);
  const Parsing::DeclarationStatement* b_declaration = GetTopDeclaration(program, 1);

  EXPECT_EQ(output.str(), "true\n");
  ASSERT_NE(a_declaration, nullptr);
  ASSERT_NE(b_declaration, nullptr);
  ASSERT_EQ(context.variables.count(a_declaration), 1);
  const auto* a_value = std::get_if<bool>(&context.variables.at(a_declaration));
  ASSERT_NE(a_value, nullptr);
  EXPECT_TRUE(*a_value);
  ASSERT_EQ(context.variables.count(b_declaration), 1);
  const auto* b_value = std::get_if<bool>(&context.variables.at(b_declaration));
  ASSERT_NE(b_value, nullptr);
  EXPECT_TRUE(*b_value);
}

TEST(InterpreterTests, UsesOuterVariableInInitializerBeforeInnerShadowing) {
  const std::string source =
      "var x int = 0;\n"
      "func main() { { var y int = x + 2; var x int = 10; print(y); } print(x); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);

  const auto* outer_x_declaration = GetTopDeclaration(program, 0);
  ASSERT_NE(outer_x_declaration, nullptr);
  ASSERT_NE(program.top_statements[1], nullptr);
  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);
  ASSERT_EQ(main_function->body->statements.size(), 2u);
  ASSERT_NE(main_function->body->statements[0], nullptr);
  const auto* block_statement =
      std::get_if<Parsing::Block>(&main_function->body->statements[0]->value);
  ASSERT_NE(block_statement, nullptr);
  ASSERT_EQ(block_statement->statements.size(), 3u);
  ASSERT_NE(block_statement->statements[0], nullptr);
  ASSERT_NE(block_statement->statements[1], nullptr);

  const auto* y_declaration =
      std::get_if<Parsing::DeclarationStatement>(&block_statement->statements[0]->value);
  ASSERT_NE(y_declaration, nullptr);
  const auto* inner_x_declaration =
      std::get_if<Parsing::DeclarationStatement>(&block_statement->statements[1]->value);
  ASSERT_NE(inner_x_declaration, nullptr);

  EXPECT_EQ(output.str(), "2\n0\n");
  ASSERT_EQ(context.variables.count(outer_x_declaration), 1);
  ASSERT_EQ(context.variables.count(y_declaration), 1);
  ASSERT_EQ(context.variables.count(inner_x_declaration), 1);
  const auto* outer_x_value =
      std::get_if<int>(&context.variables.at(outer_x_declaration));
  ASSERT_NE(outer_x_value, nullptr);
  EXPECT_EQ(*outer_x_value, 0);
  const auto* y_value = std::get_if<int>(&context.variables.at(y_declaration));
  ASSERT_NE(y_value, nullptr);
  EXPECT_EQ(*y_value, 2);
  const auto* inner_x_value =
      std::get_if<int>(&context.variables.at(inner_x_declaration));
  ASSERT_NE(inner_x_value, nullptr);
  EXPECT_EQ(*inner_x_value, 10);
}

TEST(InterpreterTests, AssignmentInsideShadowingBlockDoesNotModifyOuterVariable) {
  const std::string source =
      "var x int = 1;\n"
      "func main() { { var x int = 2; x = x + 1; } print(x); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  const Parsing::InterpreterContext context = Parsing::Interpret(program, output);

  const auto* outer_x_declaration = GetTopDeclaration(program, 0);
  ASSERT_NE(outer_x_declaration, nullptr);
  ASSERT_NE(program.top_statements[1], nullptr);
  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);
  ASSERT_EQ(main_function->body->statements.size(), 2u);
  ASSERT_NE(main_function->body->statements[0], nullptr);
  const auto* block_statement =
      std::get_if<Parsing::Block>(&main_function->body->statements[0]->value);
  ASSERT_NE(block_statement, nullptr);
  ASSERT_EQ(block_statement->statements.size(), 2u);
  ASSERT_NE(block_statement->statements[0], nullptr);
  const auto* inner_x_declaration =
      std::get_if<Parsing::DeclarationStatement>(&block_statement->statements[0]->value);
  ASSERT_NE(inner_x_declaration, nullptr);

  EXPECT_EQ(output.str(), "1\n");
  ASSERT_EQ(context.variables.count(outer_x_declaration), 1);
  ASSERT_EQ(context.variables.count(inner_x_declaration), 1);
  const auto* outer_x_value =
      std::get_if<int>(&context.variables.at(outer_x_declaration));
  ASSERT_NE(outer_x_value, nullptr);
  EXPECT_EQ(*outer_x_value, 1);
  const auto* inner_x_value =
      std::get_if<int>(&context.variables.at(inner_x_declaration));
  ASSERT_NE(inner_x_value, nullptr);
  EXPECT_EQ(*inner_x_value, 3);
}

TEST(InterpreterTests, ThrowsOnFunctionCallStatement) {
  const std::string source = "func main() { foo(); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  EXPECT_THROW(Parsing::Interpret(program, output), std::runtime_error);
}

TEST(InterpreterTests, ThrowsOnFunctionCallStatementEvenWhenFunctionIsDeclared) {
  const std::string source =
      "func foo() int { return 1; }\n"
      "func main() { foo(); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  EXPECT_THROW(Parsing::Interpret(program, output), std::runtime_error);
}

TEST(InterpreterTests, ThrowsOnFunctionCallInsideArithmeticExpression) {
  const std::string source =
      "func foo() int { return 1; }\n"
      "var x int;\n"
      "func main() { x = foo() + 1; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  EXPECT_THROW(Parsing::Interpret(program, output), std::runtime_error);
}

TEST(InterpreterTests, ThrowsOnFunctionCallInsideBoolInitializer) {
  const std::string source =
      "func foo() bool { return true; }\n"
      "var flag bool = foo();\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  EXPECT_THROW(Parsing::Interpret(program, output), std::runtime_error);
}

TEST(InterpreterTests, ThrowsOnPrintOfUnknownVariable) {
  const std::string source = "func main() { print(x); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  EXPECT_THROW(Parsing::Interpret(program, output), std::runtime_error);
}

TEST(InterpreterTests, ThrowsOnAssignmentToUndeclaredVariable) {
  const std::string source = "func main() { x = 1; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  EXPECT_THROW(Parsing::Interpret(program, output), std::runtime_error);
}

TEST(InterpreterTests, ThrowsOnDivisionByZero) {
  const std::string source =
      "var x int;\n"
      "func main() { x = 4 / (2 - 2); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  EXPECT_THROW(Parsing::Interpret(program, output), std::runtime_error);
}

TEST(InterpreterTests, DivisionLeftRecursionTest) {
  const std::string source = "func main() { print(100 / 5 / 2); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  Parsing::Interpret(program, output);
  EXPECT_EQ(output.str(), "10\n");
}

TEST(InterpreterTests, SubtractionLeftRecursionTest) {
  const std::string source = "func main() { print(100 - 5 - 2); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  std::ostringstream output;
  Parsing::Interpret(program, output);
  EXPECT_EQ(output.str(), "93\n");
}

}  // namespace
