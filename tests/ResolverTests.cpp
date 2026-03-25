#include <string>

#include <gtest/gtest.h>

#include "Parsing/Ast.hpp"
#include "Parsing/Parser.hpp"
#include "SemanticAnalysis/Resolver.hpp"
#include "SemanticAnalysis/SymbolTable.hpp"
#include "TestParseUtils.hpp"

namespace {

const Parsing::IdentifierExpression* GetIdentifierFromExpression(
    const Parsing::Expression& expression) {
  return std::get_if<Parsing::IdentifierExpression>(&expression.value);
}

TEST(ResolverTests, ResolvesOuterVariableForInitializerBeforeInnerShadowingDeclaration) {
  const std::string source =
      "var x int = 0;\n"
      "func main() { var y int = x + 2; var x int = 10; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  const Parsing::UseResolver resolver =
      Parsing::BuildUseResolver(program, symbol_table);

  ASSERT_EQ(program.top_statements.size(), 2u);
  ASSERT_NE(program.top_statements[0], nullptr);
  ASSERT_NE(program.top_statements[1], nullptr);

  const auto* outer_x_declaration =
      std::get_if<Parsing::DeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(outer_x_declaration, nullptr);

  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);
  ASSERT_EQ(main_function->body->statements.size(), 2u);
  ASSERT_NE(main_function->body->statements[0], nullptr);

  const auto* y_declaration =
      std::get_if<Parsing::DeclarationStatement>(&main_function->body->statements[0]->value);
  ASSERT_NE(y_declaration, nullptr);
  ASSERT_NE(y_declaration->initializer, nullptr);

  const auto* add_expression =
      std::get_if<Parsing::AddExpression>(&y_declaration->initializer->value);
  ASSERT_NE(add_expression, nullptr);
  ASSERT_NE(add_expression->left, nullptr);

  const auto* used_identifier = GetIdentifierFromExpression(*add_expression->left);
  ASSERT_NE(used_identifier, nullptr);

  EXPECT_EQ(
      resolver.GetUsedVarDef("x", used_identifier),
      outer_x_declaration);
  EXPECT_EQ(
      resolver.GetUsedVarDef("x", used_identifier),
      outer_x_declaration);
}

TEST(ResolverTests, ResolvesInnerShadowedVariableInsideBlock) {
  const std::string source =
      "var x int = 1;\n"
      "func main() { var x int = 2; print(x); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  const Parsing::UseResolver resolver =
      Parsing::BuildUseResolver(program, symbol_table);

  ASSERT_EQ(program.top_statements.size(), 2u);
  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);
  ASSERT_EQ(main_function->body->statements.size(), 2u);

  const auto* inner_x_declaration =
      std::get_if<Parsing::DeclarationStatement>(&main_function->body->statements[0]->value);
  ASSERT_NE(inner_x_declaration, nullptr);

  const auto* print_statement =
      std::get_if<Parsing::PrintStatement>(&main_function->body->statements[1]->value);
  ASSERT_NE(print_statement, nullptr);
  ASSERT_NE(print_statement->expr, nullptr);

  const auto* used_identifier = GetIdentifierFromExpression(*print_statement->expr);
  ASSERT_NE(used_identifier, nullptr);

  EXPECT_EQ(
      resolver.GetUsedVarDef("x", used_identifier),
      inner_x_declaration);
}

TEST(ResolverTests, ThrowsOnUseBeforeDefinitionWithoutOuterDeclaration) {
  const std::string source = "func main() { print(x); var x int = 1; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  EXPECT_THROW(
      Parsing::BuildUseResolver(program, symbol_table),
      std::runtime_error);
}

TEST(ResolverTests, ResolvesAssignmentTargetAndRhsIdentifierToInnerDeclaration) {
  const std::string source =
      "var x int = 1;\n"
      "func main() { var x int = 2; x = x + 1; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  const Parsing::UseResolver resolver =
      Parsing::BuildUseResolver(program, symbol_table);

  ASSERT_EQ(program.top_statements.size(), 2u);
  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);
  ASSERT_EQ(main_function->body->statements.size(), 2u);

  const auto* inner_x_declaration =
      std::get_if<Parsing::DeclarationStatement>(&main_function->body->statements[0]->value);
  ASSERT_NE(inner_x_declaration, nullptr);

  const auto* assignment_statement =
      std::get_if<Parsing::AssignmentStatement>(&main_function->body->statements[1]->value);
  ASSERT_NE(assignment_statement, nullptr);
  ASSERT_NE(assignment_statement->expr, nullptr);

  const auto* add_expression =
      std::get_if<Parsing::AddExpression>(&assignment_statement->expr->value);
  ASSERT_NE(add_expression, nullptr);
  ASSERT_NE(add_expression->left, nullptr);

  const auto* rhs_identifier = GetIdentifierFromExpression(*add_expression->left);
  ASSERT_NE(rhs_identifier, nullptr);

  EXPECT_EQ(
      resolver.GetUsedVarDef("x", assignment_statement),
      inner_x_declaration);
  EXPECT_EQ(
      resolver.GetUsedVarDef("x", rhs_identifier),
      inner_x_declaration);
}

TEST(ResolverTests, ResolvesFunctionCallBeforeFunctionDefinitionInSameScope) {
  const std::string source =
      "func main() { foo(); func foo() { } }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  const Parsing::UseResolver resolver =
      Parsing::BuildUseResolver(program, symbol_table);

  ASSERT_EQ(program.top_statements.size(), 1u);
  ASSERT_NE(program.top_statements[0], nullptr);

  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);
  ASSERT_EQ(main_function->body->statements.size(), 2u);
  ASSERT_NE(main_function->body->statements[0], nullptr);
  ASSERT_NE(main_function->body->statements[1], nullptr);

  const auto* call_expression =
      std::get_if<Parsing::Expression>(&main_function->body->statements[0]->value);
  ASSERT_NE(call_expression, nullptr);

  const auto* function_call =
      std::get_if<Parsing::FunctionCall>(&call_expression->value);
  ASSERT_NE(function_call, nullptr);

  const auto* function_declaration =
      std::get_if<Parsing::FunctionDeclarationStatement>(&main_function->body->statements[1]->value);
  ASSERT_NE(function_declaration, nullptr);

  EXPECT_EQ(
      resolver.GetUsedVarDef("foo", function_call),
      function_declaration);
  EXPECT_EQ(
      resolver.GetUsedVarDef("foo", function_call),
      function_declaration);
}

}  // namespace
