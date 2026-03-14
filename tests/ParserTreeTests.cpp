#include <string>

#include <gtest/gtest.h>

#include "Parsing/Ast.hpp"
#include "Parsing/Parser.hpp"
#include "Parsing/Printer.hpp"
#include "Parsing/PrinterAST.hpp"

namespace {

TEST(ParserTreeTests, BuildsDeclarationAssignmentAndPrintTree) {
  const std::string source =
      "var x int;\n"
      "x = -7 + 2 * 3;\n"
      "print(x - 1);\n";

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
  ASSERT_NE(assignment->expr, nullptr);

  const auto* assigned_add = std::get_if<Parsing::AddExpression>(&assignment->expr->value);
  ASSERT_NE(assigned_add, nullptr);
  ASSERT_NE(assigned_add->left, nullptr);
  ASSERT_NE(assigned_add->right, nullptr);

  const auto* unary_minus = std::get_if<Parsing::UnaryMinusExpression>(&assigned_add->left->value);
  ASSERT_NE(unary_minus, nullptr);
  ASSERT_NE(unary_minus->operand, nullptr);
  const auto* unary_operand = std::get_if<Parsing::NumberExpression>(&unary_minus->operand->value);
  ASSERT_NE(unary_operand, nullptr);
  EXPECT_EQ(unary_operand->value, 7);

  const auto* multiply = std::get_if<Parsing::MultiplyExpression>(&assigned_add->right->value);
  ASSERT_NE(multiply, nullptr);
  ASSERT_NE(multiply->left, nullptr);
  ASSERT_NE(multiply->right, nullptr);

  const auto* multiply_left = std::get_if<Parsing::NumberExpression>(&multiply->left->value);
  ASSERT_NE(multiply_left, nullptr);
  EXPECT_EQ(multiply_left->value, 2);

  const auto* multiply_right = std::get_if<Parsing::NumberExpression>(&multiply->right->value);
  ASSERT_NE(multiply_right, nullptr);
  EXPECT_EQ(multiply_right->value, 3);

  const auto* print_statement = std::get_if<Parsing::PrintStatement>(program.top_statements[2].get());
  ASSERT_NE(print_statement, nullptr);
  ASSERT_NE(print_statement->expr, nullptr);

  const auto* print_subtract = std::get_if<Parsing::SubtractExpression>(&print_statement->expr->value);
  ASSERT_NE(print_subtract, nullptr);
  ASSERT_NE(print_subtract->left, nullptr);
  ASSERT_NE(print_subtract->right, nullptr);

  const auto* printed_identifier = std::get_if<Parsing::IdentifierExpression>(&print_subtract->left->value);
  ASSERT_NE(printed_identifier, nullptr);
  EXPECT_EQ(printed_identifier->name, "x");

  const auto* printed_number = std::get_if<Parsing::NumberExpression>(&print_subtract->right->value);
  ASSERT_NE(printed_number, nullptr);
  EXPECT_EQ(printed_number->value, 1);
}

TEST(ParserTreeTests, BuildsIfTreeWithBlocksAndCondition) {
  const std::string source =
      "if (x + 1) * 2 >= y % 3 { print(x + y); x = x - 1; } else { print(0); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);

  ASSERT_EQ(program.top_statements.size(), 1);
  ASSERT_NE(program.top_statements[0], nullptr);

  const auto* if_statement = std::get_if<Parsing::IfStatement>(program.top_statements[0].get());
  ASSERT_NE(if_statement, nullptr);
  ASSERT_NE(if_statement->condition, nullptr);
  ASSERT_NE(if_statement->condition->left, nullptr);
  ASSERT_NE(if_statement->condition->op, nullptr);
  ASSERT_NE(if_statement->condition->right, nullptr);

  const auto* left_multiply = std::get_if<Parsing::MultiplyExpression>(&if_statement->condition->left->value);
  ASSERT_NE(left_multiply, nullptr);
  ASSERT_NE(left_multiply->left, nullptr);
  ASSERT_NE(left_multiply->right, nullptr);

  const auto* left_add = std::get_if<Parsing::AddExpression>(&left_multiply->left->value);
  ASSERT_NE(left_add, nullptr);
  ASSERT_NE(left_add->left, nullptr);
  ASSERT_NE(left_add->right, nullptr);

  const auto* add_identifier = std::get_if<Parsing::IdentifierExpression>(&left_add->left->value);
  ASSERT_NE(add_identifier, nullptr);
  EXPECT_EQ(add_identifier->name, "x");

  const auto* add_number = std::get_if<Parsing::NumberExpression>(&left_add->right->value);
  ASSERT_NE(add_number, nullptr);
  EXPECT_EQ(add_number->value, 1);

  const auto* multiply_right = std::get_if<Parsing::NumberExpression>(&left_multiply->right->value);
  ASSERT_NE(multiply_right, nullptr);
  EXPECT_EQ(multiply_right->value, 2);

  const auto* comparison = std::get_if<Parsing::GreaterEqualComparison>(if_statement->condition->op.get());
  ASSERT_NE(comparison, nullptr);

  const auto* right_modulo = std::get_if<Parsing::ModuloExpression>(&if_statement->condition->right->value);
  ASSERT_NE(right_modulo, nullptr);
  ASSERT_NE(right_modulo->left, nullptr);
  ASSERT_NE(right_modulo->right, nullptr);

  const auto* modulo_left = std::get_if<Parsing::IdentifierExpression>(&right_modulo->left->value);
  ASSERT_NE(modulo_left, nullptr);
  EXPECT_EQ(modulo_left->name, "y");

  const auto* modulo_right = std::get_if<Parsing::NumberExpression>(&right_modulo->right->value);
  ASSERT_NE(modulo_right, nullptr);
  EXPECT_EQ(modulo_right->value, 3);

  ASSERT_NE(if_statement->true_block, nullptr);
  ASSERT_NE(if_statement->else_tail, nullptr);
  ASSERT_EQ(if_statement->else_tail->else_if, nullptr);
  ASSERT_NE(if_statement->else_tail->else_block, nullptr);
  ASSERT_EQ(if_statement->true_block->statements.size(), 2);
  ASSERT_EQ(if_statement->else_tail->else_block->statements.size(), 1);
}

TEST(ParserTreeTests, BuildsElseIfChainTree) {
  const std::string source =
      "if x < 0 { print(0); } else if x < 10 { print(1); } else if x < 20 { print(2); } else { print(3); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);

  ASSERT_EQ(program.top_statements.size(), 1);
  ASSERT_NE(program.top_statements[0], nullptr);

  const auto* first_if = std::get_if<Parsing::IfStatement>(program.top_statements[0].get());
  ASSERT_NE(first_if, nullptr);
  ASSERT_NE(first_if->condition, nullptr);
  ASSERT_NE(first_if->true_block, nullptr);
  ASSERT_NE(first_if->else_tail, nullptr);
  ASSERT_NE(first_if->else_tail->else_if, nullptr);
  ASSERT_EQ(first_if->else_tail->else_block, nullptr);

  const auto* second_if = first_if->else_tail->else_if.get();
  ASSERT_NE(second_if, nullptr);
  ASSERT_NE(second_if->condition, nullptr);
  ASSERT_NE(second_if->true_block, nullptr);
  ASSERT_NE(second_if->else_tail, nullptr);
  ASSERT_NE(second_if->else_tail->else_if, nullptr);
  ASSERT_EQ(second_if->else_tail->else_block, nullptr);

  const auto* third_if = second_if->else_tail->else_if.get();
  ASSERT_NE(third_if, nullptr);
  ASSERT_NE(third_if->condition, nullptr);
  ASSERT_NE(third_if->true_block, nullptr);
  ASSERT_NE(third_if->else_tail, nullptr);
  ASSERT_EQ(third_if->else_tail->else_if, nullptr);
  ASSERT_NE(third_if->else_tail->else_block, nullptr);

  ASSERT_EQ(first_if->true_block->statements.size(), 1);
  ASSERT_EQ(second_if->true_block->statements.size(), 1);
  ASSERT_EQ(third_if->true_block->statements.size(), 1);
  ASSERT_EQ(third_if->else_tail->else_block->statements.size(), 1);
}

TEST(ParserTreeTests, BuildsIfTreeWithoutElseTail) {
  const std::string source = "if x > 0 { print(x); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);

  ASSERT_EQ(program.top_statements.size(), 1);
  ASSERT_NE(program.top_statements[0], nullptr);

  const auto* if_statement = std::get_if<Parsing::IfStatement>(program.top_statements[0].get());
  ASSERT_NE(if_statement, nullptr);
  ASSERT_NE(if_statement->condition, nullptr);
  ASSERT_NE(if_statement->true_block, nullptr);
  ASSERT_NE(if_statement->else_tail, nullptr);
  ASSERT_EQ(if_statement->else_tail->else_if, nullptr);
  ASSERT_EQ(if_statement->else_tail->else_block, nullptr);
  ASSERT_EQ(if_statement->true_block->statements.size(), 1);

  const std::string printed = Parsing::PrintInfix(program);
  EXPECT_EQ(printed, source);
}

TEST(ParserTreeTests, PrintsProgramInInfixOrder) {
  const std::string source =
      "var x int;\n"
      "x = 1 + 2 * (3 + 4);\n"
      "if x - 5 == (x + 1) / 2 { print(x % 3); } else if x < 0 { x = 0; } else if x < 2 { x = 1; } else { x = -(x - 1); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const std::string printed = Parsing::PrintInfix(program);

  const std::string expected =
      "var x int;\n"
      "x = 1 + 2 * (3 + 4);\n"
      "if x - 5 == (x + 1) / 2 { print(x % 3); } else if x < 0 { x = 0; } else if x < 2 { x = 1; } else { x = -(x - 1); }\n";

  EXPECT_EQ(printed, expected);
}

TEST(ParserTreeTests, PrintsProgramAsAstTree) {
  const std::string source =
      "var x int;\n"
      "x = -1 + 2;\n"
      "if x < 0 { print(x); } else if x == 0 { print(0); } else { x = x - 1; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const std::string tree = Parsing::PrintAstTree(program);

  const std::string expected =
      "Program:\n"
      "\tDeclarationStatement: x int\n"
      "\tAssignmentStatement: x\n"
      "\t\tAddExpression:\n"
      "\t\t\tUnaryMinusExpression:\n"
      "\t\t\t\tNumberExpression: 1\n"
      "\t\t\tNumberExpression: 2\n"
      "\tIfStatement:\n"
      "\t\tCondition:\n"
      "\t\t\tBoolExpression:\n"
      "\t\t\t\tLeft:\n"
      "\t\t\t\t\tIdentifierExpression: x\n"
      "\t\t\t\tOperator: <\n"
      "\t\t\t\tRight:\n"
      "\t\t\t\t\tNumberExpression: 0\n"
      "\t\tTrueBlock:\n"
      "\t\t\tBlock:\n"
      "\t\t\t\tPrintStatement:\n"
      "\t\t\t\t\tIdentifierExpression: x\n"
      "\t\tElseTail:\n"
      "\t\t\tElseIf:\n"
      "\t\t\t\tIfStatement:\n"
      "\t\t\t\t\tCondition:\n"
      "\t\t\t\t\t\tBoolExpression:\n"
      "\t\t\t\t\t\t\tLeft:\n"
      "\t\t\t\t\t\t\t\tIdentifierExpression: x\n"
      "\t\t\t\t\t\t\tOperator: ==\n"
      "\t\t\t\t\t\t\tRight:\n"
      "\t\t\t\t\t\t\t\tNumberExpression: 0\n"
      "\t\t\t\t\tTrueBlock:\n"
      "\t\t\t\t\t\tBlock:\n"
      "\t\t\t\t\t\t\tPrintStatement:\n"
      "\t\t\t\t\t\t\t\tNumberExpression: 0\n"
      "\t\t\t\t\tElseTail:\n"
      "\t\t\t\t\t\tElseBlock:\n"
      "\t\t\t\t\t\t\tBlock:\n"
      "\t\t\t\t\t\t\t\tAssignmentStatement: x\n"
      "\t\t\t\t\t\t\t\t\tSubtractExpression:\n"
      "\t\t\t\t\t\t\t\t\t\tIdentifierExpression: x\n"
      "\t\t\t\t\t\t\t\t\t\tNumberExpression: 1\n";

  EXPECT_EQ(tree, expected);
}

}  // namespace
