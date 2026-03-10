#include <algorithm>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "Parsing/Ast.hpp"
#include "Parsing/Parser.hpp"
#include "Parsing/Printer.hpp"

namespace {

TEST(ParserTreeTests, BuildsDeclarationAssignmentAndPrintTree) {
  const std::string source =
      "var x int;\n"
      "x = -7;\n"
      "print(x);\n";

  const Parsing::Program program = Parsing::ParseSource(source);

  ASSERT_EQ(program.top_statements.size(), 3);
  ASSERT_NE(program.top_statements[0], nullptr);
  ASSERT_NE(program.top_statements[1], nullptr);
  ASSERT_NE(program.top_statements[2], nullptr);

  const auto* declaration = std::get_if<Parsing::DeclarationStatement>(program.top_statements[0].get());
  ASSERT_NE(declaration, nullptr);
  EXPECT_EQ(declaration->variable_name, "x");
  EXPECT_EQ(declaration->type_name, "int");

  const auto* assignment = std::get_if<Parsing::AssignmentStatement>(program.top_statements[1].get());
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(assignment->variable_name, "x");
  ASSERT_NE(assignment->value, nullptr);

  const auto* assigned_number = std::get_if<Parsing::NumberExpression>(assignment->value.get());
  ASSERT_NE(assigned_number, nullptr);
  EXPECT_EQ(assigned_number->value, -7);

  const auto* print_statement = std::get_if<Parsing::PrintStatement>(program.top_statements[2].get());
  ASSERT_NE(print_statement, nullptr);
  ASSERT_NE(print_statement->value, nullptr);

  const auto* printed_identifier = std::get_if<Parsing::IdentifierExpression>(print_statement->value.get());
  ASSERT_NE(printed_identifier, nullptr);
  EXPECT_EQ(printed_identifier->name, "x");
}

TEST(ParserTreeTests, BuildsIfTreeWithBlocksAndCondition) {
  const std::string source =
      "if x >= 0 { print(x); x = 1; } else { print(0); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);

  ASSERT_EQ(program.top_statements.size(), 1);
  ASSERT_NE(program.top_statements[0], nullptr);

  const auto* if_statement = std::get_if<Parsing::IfStatement>(program.top_statements[0].get());
  ASSERT_NE(if_statement, nullptr);
  ASSERT_NE(if_statement->condition, nullptr);
  ASSERT_NE(if_statement->condition->left, nullptr);
  ASSERT_NE(if_statement->condition->op, nullptr);
  ASSERT_NE(if_statement->condition->right, nullptr);

  const auto* left_identifier = std::get_if<Parsing::IdentifierExpression>(if_statement->condition->left.get());
  ASSERT_NE(left_identifier, nullptr);
  EXPECT_EQ(left_identifier->name, "x");

  const auto* comparison = std::get_if<Parsing::GreaterEqualComparison>(if_statement->condition->op.get());
  ASSERT_NE(comparison, nullptr);

  const auto* right_number = std::get_if<Parsing::NumberExpression>(if_statement->condition->right.get());
  ASSERT_NE(right_number, nullptr);
  EXPECT_EQ(right_number->value, 0);

  ASSERT_NE(if_statement->true_block, nullptr);
  ASSERT_NE(if_statement->false_block, nullptr);
  ASSERT_EQ(if_statement->true_block->statements.size(), 2);
  ASSERT_EQ(if_statement->false_block->statements.size(), 1);
}

TEST(ParserTreeTests, PrintsProgramInInfixOrder) {
  const std::string source =
      "var x int;\n"
      "x = -10;\n"
      "if x >= 0 { print(x); } else { x = -1; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const std::string printed = Parsing::PrintInfix(program);

  const std::string expected =
      "var x int;\n"
      "x = -10;\n"
      "if x >= 0 { print(x); } else { x = -1; }";

  const auto erase_spaces = [](std::string value) {
    value.erase(std::remove(value.begin(), value.end(), ' '), value.end());
    return value;
  };

  EXPECT_EQ(erase_spaces(printed), erase_spaces(expected));
}

}  // namespace
