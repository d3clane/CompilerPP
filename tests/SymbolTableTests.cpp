#include <string>

#include <gtest/gtest.h>

#include "Parsing/Ast.hpp"
#include "Parsing/Parser.hpp"
#include "Parsing/SymbolTable.hpp"

namespace {

TEST(SymbolTableTests, BuildsScopesWithoutUseResolutionChecks) {
  const std::string source =
      "var x int = 0;\n"
      "{ var y int = x + 2; var x int = 10; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);

  ASSERT_EQ(program.top_statements.size(), 2);
  ASSERT_NE(program.top_statements[0], nullptr);
  ASSERT_NE(program.top_statements[1], nullptr);

  const auto* outer_x_declaration =
      std::get_if<Parsing::DeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(outer_x_declaration, nullptr);

  const auto* block_statement =
      std::get_if<Parsing::Block>(&program.top_statements[1]->value);
  ASSERT_NE(block_statement, nullptr);
  ASSERT_EQ(block_statement->statements.size(), 2);

  const auto* inner_x_declaration =
      std::get_if<Parsing::DeclarationStatement>(&block_statement->statements[1]->value);
  ASSERT_NE(inner_x_declaration, nullptr);

  const Parsing::SymbolData* root_x = symbol_table.GetSymbolInfo("x", program);
  ASSERT_NE(root_x, nullptr);
  EXPECT_EQ(root_x->declaration_node_id, outer_x_declaration->GetId());

  const Parsing::SymbolData* block_x = symbol_table.GetSymbolInfo("x", *block_statement);
  ASSERT_NE(block_x, nullptr);
  EXPECT_EQ(block_x->declaration_node_id, inner_x_declaration->GetId());
}

TEST(SymbolTableTests, ThrowsOnDuplicateDeclarationInSameScope) {
  const std::string source =
      "var x int;\n"
      "var x int;\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  EXPECT_THROW(Parsing::BuildSymbolTable(program), std::runtime_error);
}

TEST(SymbolTableTests, ThrowsOnIllegalShadowingOfFunction) {
  const std::string source =
      "func foo() { }\n"
      "{ var foo int; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  EXPECT_THROW(Parsing::BuildSymbolTable(program), std::runtime_error);
}

TEST(SymbolTableTests, AllowsVariableShadowingInInnerScope) {
  const std::string source =
      "var x int;\n"
      "{ var x int = 1; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  EXPECT_NO_THROW(Parsing::BuildSymbolTable(program));
}

TEST(SymbolTableTests, DoesNotCheckUseBeforeDefinitionDuringBuild) {
  const std::string source =
      "{ var y int = x + 2; var x int = 10; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);

  ASSERT_EQ(program.top_statements.size(), 1);
  const auto* block_statement =
      std::get_if<Parsing::Block>(&program.top_statements[0]->value);
  ASSERT_NE(block_statement, nullptr);
  ASSERT_EQ(block_statement->statements.size(), 2);

  const Parsing::SymbolData* symbol_x = symbol_table.GetSymbolInfo("x", *block_statement);
  ASSERT_NE(symbol_x, nullptr);
  EXPECT_EQ(symbol_x->in_scope_stmt_id, 2u);
}

TEST(SymbolTableTests, ResolvesIdentifierLookupThroughParentScope) {
  const std::string source =
      "var x int = 5;\n"
      "{ print(x); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  const auto* outer_x_declaration =
      std::get_if<Parsing::DeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(outer_x_declaration, nullptr);

  const auto* block_statement =
      std::get_if<Parsing::Block>(&program.top_statements[1]->value);
  ASSERT_NE(block_statement, nullptr);
  ASSERT_EQ(block_statement->statements.size(), 1);

  const Parsing::SymbolData* visible_x = symbol_table.GetSymbolInfo("x", *block_statement);
  ASSERT_NE(visible_x, nullptr);
  EXPECT_EQ(visible_x->name, "x");
  EXPECT_EQ(visible_x->declaration_node_id, outer_x_declaration->GetId());
}

TEST(SymbolTableTests, StoresInScopeStatementIdForDeclarations) {
  const std::string source =
      "var a int;\n"
      "var b int;\n"
      "{ var c int; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);

  const Parsing::SymbolData* symbol_a = symbol_table.GetSymbolInfo("a", program);
  ASSERT_NE(symbol_a, nullptr);
  EXPECT_EQ(symbol_a->in_scope_stmt_id, 1u);

  const Parsing::SymbolData* symbol_b = symbol_table.GetSymbolInfo("b", program);
  ASSERT_NE(symbol_b, nullptr);
  EXPECT_EQ(symbol_b->in_scope_stmt_id, 2u);

  const auto* block_statement =
      std::get_if<Parsing::Block>(&program.top_statements[2]->value);
  ASSERT_NE(block_statement, nullptr);

  const Parsing::SymbolData* symbol_c = symbol_table.GetSymbolInfo("c", *block_statement);
  ASSERT_NE(symbol_c, nullptr);
  EXPECT_EQ(symbol_c->in_scope_stmt_id, 1u);
}

}  // namespace
