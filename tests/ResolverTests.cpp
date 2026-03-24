#include <string>

#include <gtest/gtest.h>

#include "Parsing/Ast.hpp"
#include "Parsing/Parser.hpp"
#include "SemanticAnalysis/Resolver.hpp"
#include "SemanticAnalysis/SymbolTable.hpp"

namespace {

const Parsing::IdentifierExpression* GetIdentifierFromExpression(
    const Parsing::Expression& expression) {
  return std::get_if<Parsing::IdentifierExpression>(&expression.value);
}

TEST(ResolverTests, ResolvesOuterVariableForInitializerBeforeInnerShadowingDeclaration) {
  const std::string source =
      "var x int = 0;\n"
      "{ var y int = x + 2; var x int = 10; }\n";

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

  const auto* block_statement =
      std::get_if<Parsing::Block>(&program.top_statements[1]->value);
  ASSERT_NE(block_statement, nullptr);
  ASSERT_EQ(block_statement->statements.size(), 2u);
  ASSERT_NE(block_statement->statements[0], nullptr);

  const auto* y_declaration =
      std::get_if<Parsing::DeclarationStatement>(&block_statement->statements[0]->value);
  ASSERT_NE(y_declaration, nullptr);
  ASSERT_NE(y_declaration->initializer, nullptr);

  const auto* add_expression =
      std::get_if<Parsing::AddExpression>(&y_declaration->initializer->value);
  ASSERT_NE(add_expression, nullptr);
  ASSERT_NE(add_expression->left, nullptr);

  const auto* used_identifier = GetIdentifierFromExpression(*add_expression->left);
  ASSERT_NE(used_identifier, nullptr);

  EXPECT_EQ(
      resolver.GetUsedVarDef("x", const_cast<Parsing::IdentifierExpression*>(used_identifier)),
      outer_x_declaration->GetId());
  EXPECT_EQ(
      resolver.GetUsedVarDef("x", used_identifier->GetId()),
      outer_x_declaration->GetId());
}

TEST(ResolverTests, ResolvesInnerShadowedVariableInsideBlock) {
  const std::string source =
      "var x int = 1;\n"
      "{ var x int = 2; print(x); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  const Parsing::UseResolver resolver =
      Parsing::BuildUseResolver(program, symbol_table);

  ASSERT_EQ(program.top_statements.size(), 2u);
  const auto* block_statement =
      std::get_if<Parsing::Block>(&program.top_statements[1]->value);
  ASSERT_NE(block_statement, nullptr);
  ASSERT_EQ(block_statement->statements.size(), 2u);

  const auto* inner_x_declaration =
      std::get_if<Parsing::DeclarationStatement>(&block_statement->statements[0]->value);
  ASSERT_NE(inner_x_declaration, nullptr);

  const auto* print_statement =
      std::get_if<Parsing::PrintStatement>(&block_statement->statements[1]->value);
  ASSERT_NE(print_statement, nullptr);
  ASSERT_NE(print_statement->expr, nullptr);

  const auto* used_identifier = GetIdentifierFromExpression(*print_statement->expr);
  ASSERT_NE(used_identifier, nullptr);

  EXPECT_EQ(
      resolver.GetUsedVarDef("x", const_cast<Parsing::IdentifierExpression*>(used_identifier)),
      inner_x_declaration->GetId());
}

TEST(ResolverTests, ThrowsOnUseBeforeDefinitionWithoutOuterDeclaration) {
  const std::string source = "{ print(x); var x int = 1; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  EXPECT_THROW(
      Parsing::BuildUseResolver(program, symbol_table),
      std::runtime_error);
}

TEST(ResolverTests, ResolvesAssignmentTargetAndRhsIdentifierToInnerDeclaration) {
  const std::string source =
      "var x int = 1;\n"
      "{ var x int = 2; x = x + 1; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  const Parsing::UseResolver resolver =
      Parsing::BuildUseResolver(program, symbol_table);

  ASSERT_EQ(program.top_statements.size(), 2u);
  const auto* block_statement =
      std::get_if<Parsing::Block>(&program.top_statements[1]->value);
  ASSERT_NE(block_statement, nullptr);
  ASSERT_EQ(block_statement->statements.size(), 2u);

  const auto* inner_x_declaration =
      std::get_if<Parsing::DeclarationStatement>(&block_statement->statements[0]->value);
  ASSERT_NE(inner_x_declaration, nullptr);

  const auto* assignment_statement =
      std::get_if<Parsing::AssignmentStatement>(&block_statement->statements[1]->value);
  ASSERT_NE(assignment_statement, nullptr);
  ASSERT_NE(assignment_statement->expr, nullptr);

  const auto* add_expression =
      std::get_if<Parsing::AddExpression>(&assignment_statement->expr->value);
  ASSERT_NE(add_expression, nullptr);
  ASSERT_NE(add_expression->left, nullptr);

  const auto* rhs_identifier = GetIdentifierFromExpression(*add_expression->left);
  ASSERT_NE(rhs_identifier, nullptr);

  EXPECT_EQ(
      resolver.GetUsedVarDef("x", const_cast<Parsing::AssignmentStatement*>(assignment_statement)),
      inner_x_declaration->GetId());
  EXPECT_EQ(
      resolver.GetUsedVarDef("x", const_cast<Parsing::IdentifierExpression*>(rhs_identifier)),
      inner_x_declaration->GetId());
}

TEST(ResolverTests, ResolvesFunctionCallBeforeFunctionDefinitionInSameScope) {
  const std::string source =
      "foo();\n"
      "func foo() { }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  const Parsing::UseResolver resolver =
      Parsing::BuildUseResolver(program, symbol_table);

  ASSERT_EQ(program.top_statements.size(), 2u);
  ASSERT_NE(program.top_statements[0], nullptr);
  ASSERT_NE(program.top_statements[1], nullptr);

  const auto* call_expression =
      std::get_if<Parsing::Expression>(&program.top_statements[0]->value);
  ASSERT_NE(call_expression, nullptr);

  const auto* function_call =
      std::get_if<Parsing::FunctionCall>(&call_expression->value);
  ASSERT_NE(function_call, nullptr);

  const auto* function_declaration =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(function_declaration, nullptr);

  EXPECT_EQ(
      resolver.GetUsedVarDef("foo", const_cast<Parsing::FunctionCall*>(function_call)),
      function_declaration->GetId());
  EXPECT_EQ(
      resolver.GetUsedVarDef("foo", function_call->GetId()),
      function_declaration->GetId());
}

}  // namespace
