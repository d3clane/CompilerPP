#include <string>

#include <gtest/gtest.h>

#include "Parsing/Ast.hpp"
#include "Parsing/Parser.hpp"
#include "SemanticAnalysis/SymbolTable.hpp"
#include "TestParseUtils.hpp"

namespace {

const Parsing::SymbolData* FindVisibleSymbol(
    const Parsing::SymbolTable& symbol_table,
    const std::string& name,
    const Parsing::ASTNode* node) {
  const Parsing::LocalSymbolTable* scope = symbol_table.GetTable(node);
  while (scope != nullptr) {
    const Parsing::SymbolData* local_symbol =
        scope->GetSymbolInfoInLocalScope(name);
    if (local_symbol != nullptr) {
      return local_symbol;
    }

    scope = scope->GetParent();
  }

  return nullptr;
}

TEST(SymbolTableTests, BuildsScopesWithoutUseResolutionChecks) {
  const std::string source =
      "var x int = 0;\n"
      "func main() { var y int = x + 2; var x int = 10; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);

  ASSERT_EQ(program.top_statements.size(), 2);
  ASSERT_NE(program.top_statements[0], nullptr);
  ASSERT_NE(program.top_statements[1], nullptr);

  const auto* outer_x_declaration =
      std::get_if<Parsing::DeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(outer_x_declaration, nullptr);

  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);
  ASSERT_EQ(main_function->body->statements.size(), 2);

  const auto* inner_x_declaration =
      std::get_if<Parsing::DeclarationStatement>(&main_function->body->statements[1]->value);
  ASSERT_NE(inner_x_declaration, nullptr);

  const Parsing::SymbolData* root_x = FindVisibleSymbol(symbol_table, "x", &program);
  ASSERT_NE(root_x, nullptr);
  EXPECT_EQ(root_x->GetDeclarationNode(), outer_x_declaration);

  const Parsing::SymbolData* block_x =
      FindVisibleSymbol(symbol_table, "x", main_function->body.get());
  ASSERT_NE(block_x, nullptr);
  EXPECT_EQ(block_x->GetDeclarationNode(), inner_x_declaration);
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
      "func main() { var foo int; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  EXPECT_THROW(Parsing::BuildSymbolTable(program), std::runtime_error);
}

TEST(SymbolTableTests, AllowsVariableShadowingInInnerScope) {
  const std::string source =
      "var x int;\n"
      "func main() { var x int = 1; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  EXPECT_NO_THROW(Parsing::BuildSymbolTable(program));
}

TEST(SymbolTableTests, DoesNotCheckUseBeforeDefinitionDuringBuild) {
  const std::string source =
      "func main() { var y int = x + 2; var x int = 10; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);

  ASSERT_EQ(program.top_statements.size(), 1);
  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);
  ASSERT_EQ(main_function->body->statements.size(), 2);

  const Parsing::SymbolData* symbol_x =
      FindVisibleSymbol(symbol_table, "x", main_function->body.get());
  ASSERT_NE(symbol_x, nullptr);
  EXPECT_EQ(symbol_x->declaration_ref.stmt_id_in_scope, 2u);
}

TEST(SymbolTableTests, ResolvesIdentifierLookupThroughParentScope) {
  const std::string source =
      "var x int = 5;\n"
      "func main() { print(x); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  const auto* outer_x_declaration =
      std::get_if<Parsing::DeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(outer_x_declaration, nullptr);

  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);
  ASSERT_EQ(main_function->body->statements.size(), 1);

  const Parsing::SymbolData* visible_x =
      FindVisibleSymbol(symbol_table, "x", main_function->body.get());
  ASSERT_NE(visible_x, nullptr);
  EXPECT_EQ(visible_x->GetDeclarationNode(), outer_x_declaration);
}

TEST(SymbolTableTests, StoresInScopeStatementIdForDeclarations) {
  const std::string source =
      "var a int;\n"
      "var b int;\n"
      "func main() { var c int; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);

  const Parsing::SymbolData* symbol_a = FindVisibleSymbol(symbol_table, "a", &program);
  ASSERT_NE(symbol_a, nullptr);
  EXPECT_EQ(symbol_a->declaration_ref.stmt_id_in_scope, 1u);

  const Parsing::SymbolData* symbol_b = FindVisibleSymbol(symbol_table, "b", &program);
  ASSERT_NE(symbol_b, nullptr);
  EXPECT_EQ(symbol_b->declaration_ref.stmt_id_in_scope, 2u);

  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[2]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);

  const Parsing::SymbolData* symbol_c =
      FindVisibleSymbol(symbol_table, "c", main_function->body.get());
  ASSERT_NE(symbol_c, nullptr);
  EXPECT_EQ(symbol_c->declaration_ref.stmt_id_in_scope, 1u);
}

TEST(SymbolTableTests, StoresClassTypedVariables) {
  const std::string source =
      "class Node { var value int; }\n"
      "var root Node;\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);

  const Parsing::SymbolData* root_symbol = FindVisibleSymbol(symbol_table, "root", &program);
  ASSERT_NE(root_symbol, nullptr);
  EXPECT_EQ(root_symbol->kind, Parsing::SymbolKind::Variable);
  const auto* class_type = Parsing::AsClassType(root_symbol->type);
  ASSERT_NE(class_type, nullptr);
  ASSERT_NE(class_type->parent, nullptr);
  EXPECT_EQ(class_type->parent->class_name.name, "Node");
}

TEST(SymbolTableTests, ReturnsClassDeclarationOwnerForClassScope) {
  const std::string source =
      "class Node { var value int; }\n"
      "func main() { var x int; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);

  ASSERT_EQ(program.top_statements.size(), 2u);
  ASSERT_NE(program.top_statements[0], nullptr);
  ASSERT_NE(program.top_statements[1], nullptr);
  const auto* class_declaration =
      std::get_if<Parsing::ClassDeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(class_declaration, nullptr);
  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);

  const Parsing::LocalSymbolTable* class_scope = symbol_table.GetTable(class_declaration);
  ASSERT_NE(class_scope, nullptr);
  EXPECT_EQ(symbol_table.GetIfClassDeclarationOwner(class_scope), class_declaration);

  const Parsing::LocalSymbolTable* main_scope = symbol_table.GetTable(main_function->body.get());
  ASSERT_NE(main_scope, nullptr);
  EXPECT_EQ(symbol_table.GetIfClassDeclarationOwner(main_scope), nullptr);
}

TEST(SymbolTableTests, ReturnsFunctionDeclarationOwnerForFunctionBodyScope) {
  const std::string source =
      "func main() { var x int; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);

  ASSERT_EQ(program.top_statements.size(), 1u);
  ASSERT_NE(program.top_statements[0], nullptr);
  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);

  const Parsing::LocalSymbolTable* main_scope = symbol_table.GetTable(main_function->body.get());
  ASSERT_NE(main_scope, nullptr);
  EXPECT_EQ(symbol_table.GetIfFunctionDeclarationOwner(main_scope), main_function);
  EXPECT_EQ(symbol_table.GetScopeOwner(main_scope), static_cast<const Parsing::ASTNode*>(main_function));
}

TEST(SymbolTableTests, ReturnsIfAndElseOwnersForBranchScopes) {
  const std::string source =
      "func main() { if true { var x int; } else { var y int; } }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);

  ASSERT_EQ(program.top_statements.size(), 1u);
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
  ASSERT_NE(if_statement->true_block, nullptr);
  ASSERT_NE(if_statement->else_tail, nullptr);
  ASSERT_NE(if_statement->else_tail->else_block, nullptr);

  const Parsing::LocalSymbolTable* true_scope =
      symbol_table.GetTable(if_statement->true_block.get());
  ASSERT_NE(true_scope, nullptr);
  EXPECT_EQ(
      dynamic_cast<const Parsing::IfStatement*>(symbol_table.GetScopeOwner(true_scope)),
      if_statement);

  const Parsing::LocalSymbolTable* else_scope =
      symbol_table.GetTable(if_statement->else_tail->else_block.get());
  ASSERT_NE(else_scope, nullptr);
  EXPECT_EQ(
      dynamic_cast<const Parsing::ElseTail*>(symbol_table.GetScopeOwner(else_scope)),
      if_statement->else_tail.get());
}

TEST(SymbolTableTests, AllowsShadowingClassName) {
  const std::string source =
      "class A { }\n"
      "func main() { var A int; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  EXPECT_NO_THROW(Parsing::BuildSymbolTable(program));
}

TEST(SymbolTableTests, BuildsSymbolTableForMethodCallReceiverUseContext) {
  const std::string source =
      "class A { func ping() { } }\n"
      "var a A;\n"
      "func main() { a.ping(); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);

  ASSERT_EQ(program.top_statements.size(), 3u);
  ASSERT_NE(program.top_statements[2], nullptr);
  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[2]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);
  ASSERT_EQ(main_function->body->statements.size(), 1u);
  ASSERT_NE(main_function->body->statements[0], nullptr);

  const auto* expression_statement =
      std::get_if<Parsing::Expression>(&main_function->body->statements[0]->value);
  ASSERT_NE(expression_statement, nullptr);

  const auto* method_call =
      std::get_if<Parsing::MethodCall>(&expression_statement->value);
  ASSERT_NE(method_call, nullptr);
  EXPECT_NE(symbol_table.GetTable(method_call), nullptr);
}

TEST(SymbolTableTests, BuildsSymbolTableForFieldAccessReceiverUseContext) {
  const std::string source =
      "class A { var value int; func get(other A) int { return other.value; } }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);

  ASSERT_EQ(program.top_statements.size(), 1u);
  ASSERT_NE(program.top_statements[0], nullptr);
  const auto* class_declaration =
      std::get_if<Parsing::ClassDeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(class_declaration, nullptr);
  ASSERT_EQ(class_declaration->methods.size(), 1u);
  ASSERT_NE(class_declaration->methods[0].body, nullptr);
  ASSERT_EQ(class_declaration->methods[0].body->statements.size(), 1u);
  ASSERT_NE(class_declaration->methods[0].body->statements[0], nullptr);

  const auto* return_statement =
      std::get_if<Parsing::ReturnStatement>(&class_declaration->methods[0].body->statements[0]->value);
  ASSERT_NE(return_statement, nullptr);
  ASSERT_NE(return_statement->expr, nullptr);

  const auto* field_access =
      std::get_if<Parsing::FieldAccess>(&return_statement->expr->value);
  ASSERT_NE(field_access, nullptr);
  EXPECT_NE(symbol_table.GetTable(field_access), nullptr);
}

TEST(SymbolTableTests, DerivedClassScopeDoesNotContainInheritedBaseField) {
  const std::string source =
      "class Base { var value int; }\n"
      "class Derived:Base { func noop() { } }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);

  ASSERT_EQ(program.top_statements.size(), 2u);
  ASSERT_NE(program.top_statements[0], nullptr);
  ASSERT_NE(program.top_statements[1], nullptr);

  const auto* base_class =
      std::get_if<Parsing::ClassDeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(base_class, nullptr);
  ASSERT_EQ(base_class->fields.size(), 1u);

  const auto* derived_class =
      std::get_if<Parsing::ClassDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(derived_class, nullptr);

  const Parsing::LocalSymbolTable* derived_scope = symbol_table.GetTable(derived_class);
  ASSERT_NE(derived_scope, nullptr);
  EXPECT_EQ(derived_scope->GetSymbolInfoInLocalScope("value"), nullptr);
}

TEST(SymbolTableTests, DerivedClassMethodKeepsLocalOverride) {
  const std::string source =
      "class Base { func ping() int { return 1; } }\n"
      "class Derived:Base { func ping() int { return 2; } }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);

  ASSERT_EQ(program.top_statements.size(), 2u);
  ASSERT_NE(program.top_statements[0], nullptr);
  ASSERT_NE(program.top_statements[1], nullptr);
  const auto* base_class =
      std::get_if<Parsing::ClassDeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(base_class, nullptr);
  ASSERT_EQ(base_class->methods.size(), 1u);
  const auto* derived_class =
      std::get_if<Parsing::ClassDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(derived_class, nullptr);
  ASSERT_EQ(derived_class->methods.size(), 1u);

  const Parsing::LocalSymbolTable* derived_scope = symbol_table.GetTable(derived_class);
  ASSERT_NE(derived_scope, nullptr);
  const Parsing::SymbolData* ping_symbol =
      derived_scope->GetSymbolInfoInLocalScope("ping");
  ASSERT_NE(ping_symbol, nullptr);
  EXPECT_EQ(ping_symbol->GetDeclarationNode(), &derived_class->methods[0]);
}

TEST(SymbolTableTests, AllowsDerivedClassToRedefineInheritedField) {
  const std::string source =
      "class Base { var x int; }\n"
      "class Derived:Base { var x int; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);

  ASSERT_EQ(program.top_statements.size(), 2u);
  ASSERT_NE(program.top_statements[1], nullptr);
  const auto* derived_class =
      std::get_if<Parsing::ClassDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(derived_class, nullptr);
  ASSERT_EQ(derived_class->fields.size(), 1u);

  const Parsing::LocalSymbolTable* derived_scope = symbol_table.GetTable(derived_class);
  ASSERT_NE(derived_scope, nullptr);
  const Parsing::SymbolData* x_symbol =
      derived_scope->GetSymbolInfoInLocalScope("x");
  ASSERT_NE(x_symbol, nullptr);
  EXPECT_EQ(x_symbol->GetDeclarationNode(), &derived_class->fields[0]);
}

TEST(SymbolTableTests, SupportsInheritanceWhenBaseDeclaredAfterDerived) {
  const std::string source =
      "class Derived:Base { func noop() { } }\n"
      "class Base { var value int; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);

  ASSERT_EQ(program.top_statements.size(), 2u);
  ASSERT_NE(program.top_statements[0], nullptr);
  ASSERT_NE(program.top_statements[1], nullptr);

  const auto* derived_class =
      std::get_if<Parsing::ClassDeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(derived_class, nullptr);

  const auto* base_class =
      std::get_if<Parsing::ClassDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(base_class, nullptr);
  ASSERT_EQ(base_class->fields.size(), 1u);

  const Parsing::LocalSymbolTable* derived_scope = symbol_table.GetTable(derived_class);
  ASSERT_NE(derived_scope, nullptr);
  EXPECT_EQ(derived_scope->GetSymbolInfoInLocalScope("value"), nullptr);
}

TEST(SymbolTableTests, SupportsMethodOverrideWhenBaseDeclaredAfterDerived) {
  const std::string source =
      "class Derived:Base { func ping() int { return 2; } }\n"
      "class Base { func ping() int { return 1; } }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);

  ASSERT_EQ(program.top_statements.size(), 2u);
  ASSERT_NE(program.top_statements[0], nullptr);
  ASSERT_NE(program.top_statements[1], nullptr);
  const auto* derived_class =
      std::get_if<Parsing::ClassDeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(derived_class, nullptr);
  ASSERT_EQ(derived_class->methods.size(), 1u);
  const auto* base_class =
      std::get_if<Parsing::ClassDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(base_class, nullptr);
  ASSERT_EQ(base_class->methods.size(), 1u);

  const Parsing::LocalSymbolTable* derived_scope = symbol_table.GetTable(derived_class);
  ASSERT_NE(derived_scope, nullptr);
  const Parsing::SymbolData* method_symbol =
      derived_scope->GetSymbolInfoInLocalScope("ping");
  ASSERT_NE(method_symbol, nullptr);
  EXPECT_EQ(method_symbol->GetDeclarationNode(), &derived_class->methods[0]);
}

TEST(SymbolTableTests, KeepsMostDerivedMethodDeclarationAcrossInheritanceChain) {
  const std::string source =
      "class A { func ping() int { return 1; } }\n"
      "class B:A { func ping() int { return 2; } }\n"
      "class C:B { func ping() int { return 3; } }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);

  ASSERT_EQ(program.top_statements.size(), 3u);
  ASSERT_NE(program.top_statements[0], nullptr);
  ASSERT_NE(program.top_statements[2], nullptr);

  const auto* base_class =
      std::get_if<Parsing::ClassDeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(base_class, nullptr);
  ASSERT_EQ(base_class->methods.size(), 1u);

  const auto* derived_class =
      std::get_if<Parsing::ClassDeclarationStatement>(&program.top_statements[2]->value);
  ASSERT_NE(derived_class, nullptr);
  ASSERT_EQ(derived_class->methods.size(), 1u);

  const Parsing::LocalSymbolTable* derived_scope = symbol_table.GetTable(derived_class);
  ASSERT_NE(derived_scope, nullptr);
  const Parsing::SymbolData* method_symbol =
      derived_scope->GetSymbolInfoInLocalScope("ping");
  ASSERT_NE(method_symbol, nullptr);
  EXPECT_EQ(method_symbol->GetDeclarationNode(), &derived_class->methods[0]);
}

}  // namespace
