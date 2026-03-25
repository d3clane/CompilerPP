#include <string>

#include <gtest/gtest.h>

#include "Parsing/Ast.hpp"
#include "Parsing/Parser.hpp"
#include "Visitors/Printer.hpp"
#include "Visitors/PrinterAST.hpp"

namespace {

template <typename T>
const T* GetExpressionNode(const std::unique_ptr<Parsing::Expression>& expression) {
  if (expression == nullptr) {
    return nullptr;
  }

  return std::get_if<T>(&expression->value);
}

const int* GetIntLiteralValue(const std::unique_ptr<Parsing::Expression>& expression) {
  const auto* literal = GetExpressionNode<Parsing::LiteralExpression>(expression);
  if (literal == nullptr) {
    return nullptr;
  }
  return std::get_if<int>(&literal->value);
}

TEST(ParserTreeTests, ParsesIntDeclarationAssignmentAndPrint) {
  const std::string source =
      "var x int;\n"
      "func main() { x = -7 + 2 * 3; print(x - 1); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);

  ASSERT_EQ(program.top_statements.size(), 2);
  ASSERT_NE(program.top_statements[0], nullptr);
  ASSERT_NE(program.top_statements[1], nullptr);

  const auto* declaration =
      std::get_if<Parsing::DeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(declaration, nullptr);
  EXPECT_EQ(declaration->variable_name, "x");
  EXPECT_TRUE(std::holds_alternative<Parsing::IntType>(declaration->type.type));
  EXPECT_FALSE(declaration->is_mutable);
  EXPECT_EQ(declaration->initializer, nullptr);

  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);
  ASSERT_EQ(main_function->body->statements.size(), 2u);
  ASSERT_NE(main_function->body->statements[0], nullptr);
  ASSERT_NE(main_function->body->statements[1], nullptr);

  const auto* assignment =
      std::get_if<Parsing::AssignmentStatement>(&main_function->body->statements[0]->value);
  ASSERT_NE(assignment, nullptr);
  EXPECT_EQ(assignment->variable_name, "x");
  ASSERT_NE(assignment->expr, nullptr);

  const auto* assigned_add = GetExpressionNode<Parsing::AddExpression>(assignment->expr);
  ASSERT_NE(assigned_add, nullptr);
  ASSERT_NE(assigned_add->left, nullptr);
  ASSERT_NE(assigned_add->right, nullptr);

  const auto* unary_minus = GetExpressionNode<Parsing::UnaryMinusExpression>(assigned_add->left);
  ASSERT_NE(unary_minus, nullptr);
  ASSERT_NE(unary_minus->operand, nullptr);
  const auto* unary_operand = GetExpressionNode<Parsing::LiteralExpression>(unary_minus->operand);
  ASSERT_NE(unary_operand, nullptr);
  const auto* unary_operand_value = std::get_if<int>(&unary_operand->value);
  ASSERT_NE(unary_operand_value, nullptr);
  EXPECT_EQ(*unary_operand_value, 7);

  const auto* multiply = GetExpressionNode<Parsing::MultiplyExpression>(assigned_add->right);
  ASSERT_NE(multiply, nullptr);
  ASSERT_NE(multiply->left, nullptr);
  ASSERT_NE(multiply->right, nullptr);

  const auto* multiply_left = GetExpressionNode<Parsing::LiteralExpression>(multiply->left);
  ASSERT_NE(multiply_left, nullptr);
  const auto* multiply_left_value = std::get_if<int>(&multiply_left->value);
  ASSERT_NE(multiply_left_value, nullptr);
  EXPECT_EQ(*multiply_left_value, 2);

  const auto* multiply_right = GetExpressionNode<Parsing::LiteralExpression>(multiply->right);
  ASSERT_NE(multiply_right, nullptr);
  const auto* multiply_right_value = std::get_if<int>(&multiply_right->value);
  ASSERT_NE(multiply_right_value, nullptr);
  EXPECT_EQ(*multiply_right_value, 3);

  const auto* print_statement =
      std::get_if<Parsing::PrintStatement>(&main_function->body->statements[1]->value);
  ASSERT_NE(print_statement, nullptr);
  ASSERT_NE(print_statement->expr, nullptr);

  const auto* print_subtract =
      GetExpressionNode<Parsing::SubtractExpression>(print_statement->expr);
  ASSERT_NE(print_subtract, nullptr);
  ASSERT_NE(print_subtract->left, nullptr);
  ASSERT_NE(print_subtract->right, nullptr);

  const auto* printed_identifier = GetExpressionNode<Parsing::IdentifierExpression>(print_subtract->left);
  ASSERT_NE(printed_identifier, nullptr);
  EXPECT_EQ(printed_identifier->name, "x");

  const auto* printed_number = GetExpressionNode<Parsing::LiteralExpression>(print_subtract->right);
  ASSERT_NE(printed_number, nullptr);
  const auto* printed_number_value = std::get_if<int>(&printed_number->value);
  ASSERT_NE(printed_number_value, nullptr);
  EXPECT_EQ(*printed_number_value, 1);
}

TEST(ParserTreeTests, ParsesLogicalIfTree) {
  const std::string source =
      "var flag bool = true;\n"
      "func main() { if !flag || 1 < 2 && flag { print(flag); } else { print(false); } }\n";

  const Parsing::Program program = Parsing::ParseSource(source);

  ASSERT_EQ(program.top_statements.size(), 2);
  ASSERT_NE(program.top_statements[1], nullptr);

  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);
  ASSERT_EQ(main_function->body->statements.size(), 1u);
  ASSERT_NE(main_function->body->statements[0], nullptr);

  const auto* if_statement =
      std::get_if<Parsing::IfStatement>(&main_function->body->statements[0]->value);
  ASSERT_NE(if_statement, nullptr);
  ASSERT_NE(if_statement->condition, nullptr);
  ASSERT_NE(if_statement->true_block, nullptr);
  ASSERT_NE(if_statement->else_tail, nullptr);

  const auto* condition_or = GetExpressionNode<Parsing::LogicalOrExpression>(if_statement->condition);
  ASSERT_NE(condition_or, nullptr);
  ASSERT_NE(condition_or->left, nullptr);
  ASSERT_NE(condition_or->right, nullptr);

  const auto* left_not = GetExpressionNode<Parsing::UnaryNotExpression>(condition_or->left);
  ASSERT_NE(left_not, nullptr);
  ASSERT_NE(left_not->operand, nullptr);

  const auto* negated_identifier =
      GetExpressionNode<Parsing::IdentifierExpression>(left_not->operand);
  ASSERT_NE(negated_identifier, nullptr);
  EXPECT_EQ(negated_identifier->name, "flag");

  const auto* right_and = GetExpressionNode<Parsing::LogicalAndExpression>(condition_or->right);
  ASSERT_NE(right_and, nullptr);
  ASSERT_NE(right_and->left, nullptr);
  ASSERT_NE(right_and->right, nullptr);

  const auto* comparison = GetExpressionNode<Parsing::LessExpression>(right_and->left);
  ASSERT_NE(comparison, nullptr);
  ASSERT_NE(comparison->left, nullptr);
  ASSERT_NE(comparison->right, nullptr);

  const auto* right_identifier =
      GetExpressionNode<Parsing::IdentifierExpression>(right_and->right);
  ASSERT_NE(right_identifier, nullptr);
  EXPECT_EQ(right_identifier->name, "flag");
}

TEST(ParserTreeTests, ParsesComplexIfTreeWithBlocksAndCondition) {
  const std::string source =
      "func main() { if (x + 1) * 2 >= y % 3 { print(x + y); x = x - 1; } else { print(0); } }\n";

  const Parsing::Program program = Parsing::ParseSource(source);

  ASSERT_EQ(program.top_statements.size(), 1);
  ASSERT_NE(program.top_statements[0], nullptr);

  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);
  ASSERT_EQ(main_function->body->statements.size(), 1u);
  ASSERT_NE(main_function->body->statements[0], nullptr);

  const auto* if_statement =
      std::get_if<Parsing::IfStatement>(&main_function->body->statements[0]->value);
  ASSERT_NE(if_statement, nullptr);
  ASSERT_NE(if_statement->condition, nullptr);
  ASSERT_NE(if_statement->true_block, nullptr);
  ASSERT_NE(if_statement->else_tail, nullptr);

  const auto* comparison =
      GetExpressionNode<Parsing::GreaterEqualExpression>(if_statement->condition);
  ASSERT_NE(comparison, nullptr);
  ASSERT_NE(comparison->left, nullptr);
  ASSERT_NE(comparison->right, nullptr);

  const auto* left_multiply =
      GetExpressionNode<Parsing::MultiplyExpression>(comparison->left);
  ASSERT_NE(left_multiply, nullptr);
  ASSERT_NE(left_multiply->left, nullptr);
  ASSERT_NE(left_multiply->right, nullptr);

  const auto* left_add = GetExpressionNode<Parsing::AddExpression>(left_multiply->left);
  ASSERT_NE(left_add, nullptr);
  ASSERT_NE(left_add->left, nullptr);
  ASSERT_NE(left_add->right, nullptr);

  const auto* add_identifier =
      GetExpressionNode<Parsing::IdentifierExpression>(left_add->left);
  ASSERT_NE(add_identifier, nullptr);
  EXPECT_EQ(add_identifier->name, "x");

  const int* add_right_value = GetIntLiteralValue(left_add->right);
  ASSERT_NE(add_right_value, nullptr);
  EXPECT_EQ(*add_right_value, 1);

  const int* multiply_right_value = GetIntLiteralValue(left_multiply->right);
  ASSERT_NE(multiply_right_value, nullptr);
  EXPECT_EQ(*multiply_right_value, 2);

  const auto* right_modulo = GetExpressionNode<Parsing::ModuloExpression>(comparison->right);
  ASSERT_NE(right_modulo, nullptr);
  ASSERT_NE(right_modulo->left, nullptr);
  ASSERT_NE(right_modulo->right, nullptr);

  const auto* modulo_left =
      GetExpressionNode<Parsing::IdentifierExpression>(right_modulo->left);
  ASSERT_NE(modulo_left, nullptr);
  EXPECT_EQ(modulo_left->name, "y");

  const int* modulo_right_value = GetIntLiteralValue(right_modulo->right);
  ASSERT_NE(modulo_right_value, nullptr);
  EXPECT_EQ(*modulo_right_value, 3);

  ASSERT_EQ(if_statement->true_block->statements.size(), 2);
  ASSERT_EQ(if_statement->else_tail->else_if, nullptr);
  ASSERT_NE(if_statement->else_tail->else_block, nullptr);
  ASSERT_EQ(if_statement->else_tail->else_block->statements.size(), 1);
}

TEST(ParserTreeTests, ParsesElseIfChainTree) {
  const std::string source =
      "func main() { if x < 0 { print(0); } else if x < 10 { print(1); } else if x < 20 { print(2); } else { print(3); } }\n";

  const Parsing::Program program = Parsing::ParseSource(source);

  ASSERT_EQ(program.top_statements.size(), 1);
  ASSERT_NE(program.top_statements[0], nullptr);

  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);
  ASSERT_EQ(main_function->body->statements.size(), 1u);
  ASSERT_NE(main_function->body->statements[0], nullptr);

  const auto* first_if = std::get_if<Parsing::IfStatement>(&main_function->body->statements[0]->value);
  ASSERT_NE(first_if, nullptr);
  ASSERT_NE(first_if->condition, nullptr);
  ASSERT_NE(first_if->true_block, nullptr);
  ASSERT_NE(first_if->else_tail, nullptr);
  ASSERT_NE(first_if->else_tail->else_if, nullptr);
  ASSERT_EQ(first_if->else_tail->else_block, nullptr);

  const auto* first_condition = GetExpressionNode<Parsing::LessExpression>(first_if->condition);
  ASSERT_NE(first_condition, nullptr);
  const int* first_condition_right = GetIntLiteralValue(first_condition->right);
  ASSERT_NE(first_condition_right, nullptr);
  EXPECT_EQ(*first_condition_right, 0);

  const auto* second_if = first_if->else_tail->else_if.get();
  ASSERT_NE(second_if, nullptr);
  ASSERT_NE(second_if->condition, nullptr);
  ASSERT_NE(second_if->true_block, nullptr);
  ASSERT_NE(second_if->else_tail, nullptr);
  ASSERT_NE(second_if->else_tail->else_if, nullptr);
  ASSERT_EQ(second_if->else_tail->else_block, nullptr);

  const auto* second_condition =
      GetExpressionNode<Parsing::LessExpression>(second_if->condition);
  ASSERT_NE(second_condition, nullptr);
  const int* second_condition_right = GetIntLiteralValue(second_condition->right);
  ASSERT_NE(second_condition_right, nullptr);
  EXPECT_EQ(*second_condition_right, 10);

  const auto* third_if = second_if->else_tail->else_if.get();
  ASSERT_NE(third_if, nullptr);
  ASSERT_NE(third_if->condition, nullptr);
  ASSERT_NE(third_if->true_block, nullptr);
  ASSERT_NE(third_if->else_tail, nullptr);
  ASSERT_EQ(third_if->else_tail->else_if, nullptr);
  ASSERT_NE(third_if->else_tail->else_block, nullptr);

  const auto* third_condition =
      GetExpressionNode<Parsing::LessExpression>(third_if->condition);
  ASSERT_NE(third_condition, nullptr);
  const int* third_condition_right = GetIntLiteralValue(third_condition->right);
  ASSERT_NE(third_condition_right, nullptr);
  EXPECT_EQ(*third_condition_right, 20);

  ASSERT_EQ(first_if->true_block->statements.size(), 1);
  ASSERT_EQ(second_if->true_block->statements.size(), 1);
  ASSERT_EQ(third_if->true_block->statements.size(), 1);
  ASSERT_EQ(third_if->else_tail->else_block->statements.size(), 1);
}

TEST(ParserTreeTests, ParsesIfTreeWithoutElseTail) {
  const std::string source = "func main() { if x > 0 { print(x); } }\n";

  const Parsing::Program program = Parsing::ParseSource(source);

  ASSERT_EQ(program.top_statements.size(), 1);
  ASSERT_NE(program.top_statements[0], nullptr);

  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);
  ASSERT_EQ(main_function->body->statements.size(), 1u);
  ASSERT_NE(main_function->body->statements[0], nullptr);

  const auto* if_statement =
      std::get_if<Parsing::IfStatement>(&main_function->body->statements[0]->value);
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

TEST(ParserTreeTests, ParsesFunctionDeclarationAndNamedCall) {
  const std::string source =
      "func foo(a int, b bool) int { return a; }\n"
      "func main() { foo(x: 1, y: true); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);

  ASSERT_EQ(program.top_statements.size(), 2);
  ASSERT_NE(program.top_statements[0], nullptr);
  ASSERT_NE(program.top_statements[1], nullptr);

  const auto* function_decl =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(function_decl, nullptr);
  EXPECT_EQ(function_decl->function_name, "foo");
  ASSERT_EQ(function_decl->parameters.size(), 2);
  EXPECT_EQ(function_decl->parameters[0].name, "a");
  EXPECT_TRUE(std::holds_alternative<Parsing::IntType>(function_decl->parameters[0].type.type));
  EXPECT_EQ(function_decl->parameters[1].name, "b");
  EXPECT_TRUE(std::holds_alternative<Parsing::BoolType>(function_decl->parameters[1].type.type));
  ASSERT_TRUE(function_decl->return_type.has_value());
  EXPECT_TRUE(std::holds_alternative<Parsing::IntType>(function_decl->return_type->type));
  ASSERT_NE(function_decl->body, nullptr);
  ASSERT_EQ(function_decl->body->statements.size(), 1);

  const auto* return_statement =
      std::get_if<Parsing::ReturnStatement>(&function_decl->body->statements[0]->value);
  ASSERT_NE(return_statement, nullptr);
  ASSERT_NE(return_statement->expr, nullptr);

  const auto* returned_identifier =
      GetExpressionNode<Parsing::IdentifierExpression>(return_statement->expr);
  ASSERT_NE(returned_identifier, nullptr);
  EXPECT_EQ(returned_identifier->name, "a");

  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);
  ASSERT_EQ(main_function->body->statements.size(), 1u);
  ASSERT_NE(main_function->body->statements[0], nullptr);

  const auto* expression_statement =
      std::get_if<Parsing::Expression>(&main_function->body->statements[0]->value);
  ASSERT_NE(expression_statement, nullptr);

  const auto* function_call = std::get_if<Parsing::FunctionCall>(&expression_statement->value);
  ASSERT_NE(function_call, nullptr);
  EXPECT_EQ(function_call->function_name, "foo");
  ASSERT_EQ(function_call->arguments.size(), 2);

  const auto* first_named =
      std::get_if<Parsing::NamedCallArgument>(function_call->arguments[0].get());
  ASSERT_NE(first_named, nullptr);
  EXPECT_EQ(first_named->name, "x");
  ASSERT_NE(first_named->value, nullptr);

  const auto* first_named_number =
      GetExpressionNode<Parsing::LiteralExpression>(first_named->value);
  ASSERT_NE(first_named_number, nullptr);
  const auto* first_named_number_value = std::get_if<int>(&first_named_number->value);
  ASSERT_NE(first_named_number_value, nullptr);
  EXPECT_EQ(*first_named_number_value, 1);

  const auto* second_named =
      std::get_if<Parsing::NamedCallArgument>(function_call->arguments[1].get());
  ASSERT_NE(second_named, nullptr);
  EXPECT_EQ(second_named->name, "y");
  ASSERT_NE(second_named->value, nullptr);

  const auto* second_named_bool =
      GetExpressionNode<Parsing::LiteralExpression>(second_named->value);
  ASSERT_NE(second_named_bool, nullptr);
  const auto* second_named_bool_value = std::get_if<bool>(&second_named_bool->value);
  ASSERT_NE(second_named_bool_value, nullptr);
  EXPECT_TRUE(*second_named_bool_value);
}

TEST(ParserTreeTests, PrintsProgramInInfixOrder) {
  const std::string source =
      "var mutable x int = 1 + 2;\n"
      "var flag bool = x > 0 && true;\n"
      "func choose(a int) int { return a; }\n"
      "func main() { if flag { print(x); } else { print(false); } choose(a: x); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const std::string printed = Parsing::PrintInfix(program);

  EXPECT_EQ(printed, source);
}

TEST(ParserTreeTests, PrintsProgramAsAstTree) {
  const std::string source =
      "var flag bool = true;\n"
      "func foo(a int) int { return a; }\n"
      "func main() { foo(a: 1); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const std::string tree = Parsing::PrintAstTree(program);

  EXPECT_NE(tree.find("DeclarationStatement: flag bool"), std::string::npos);
  EXPECT_NE(tree.find("FunctionDeclarationStatement: foo"), std::string::npos);
  EXPECT_NE(tree.find("ReturnStatement:"), std::string::npos);
  EXPECT_NE(tree.find("ExpressionStatement:"), std::string::npos);
  EXPECT_NE(tree.find("FunctionCallExpression:"), std::string::npos);
  EXPECT_NE(tree.find("NamedArgument: a"), std::string::npos);
}

TEST(ParserTreeTests, ReportsMultipleLexerAndParserErrorsTogether) {
  const std::string source =
      "var x int = ;\n"
      "func main( int ) {\n"
      "  @@@\n"
      "}\n";

  try {
    static_cast<void>(Parsing::ParseSource(source, "broken.cgor"));
    FAIL() << "Expected ParseSource to throw aggregated frontend errors";
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    EXPECT_NE(message.find("Tokenizing error"), std::string::npos);
    EXPECT_NE(message.find("Parse error"), std::string::npos);
    EXPECT_NE(message.find("broken.cgor"), std::string::npos);
  }
}

}  // namespace
