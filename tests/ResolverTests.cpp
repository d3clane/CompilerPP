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

TEST(ResolverTests, ThrowsOnFunctionCallBeforeFunctionDefinitionInLocalScope) {
  const std::string source =
      "func main() { foo(); func foo() { } }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  EXPECT_THROW(
      Parsing::BuildUseResolver(program, symbol_table),
      std::runtime_error);
}

TEST(ResolverTests, ResolvesFunctionCallBeforeFunctionDefinitionInGlobalScope) {
  const std::string source =
      "func main() { foo(); }\n"
      "func foo() { }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  const Parsing::UseResolver resolver =
      Parsing::BuildUseResolver(program, symbol_table);

  ASSERT_EQ(program.top_statements.size(), 2u);
  ASSERT_NE(program.top_statements[0], nullptr);
  ASSERT_NE(program.top_statements[1], nullptr);

  const auto* main_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(main_function, nullptr);
  ASSERT_NE(main_function->body, nullptr);
  ASSERT_EQ(main_function->body->statements.size(), 1u);
  ASSERT_NE(main_function->body->statements[0], nullptr);

  const auto* call_expression =
      std::get_if<Parsing::Expression>(&main_function->body->statements[0]->value);
  ASSERT_NE(call_expression, nullptr);

  const auto* function_call =
      std::get_if<Parsing::FunctionCall>(&call_expression->value);
  ASSERT_NE(function_call, nullptr);

  const auto* function_declaration =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(function_declaration, nullptr);

  EXPECT_EQ(
      resolver.GetUsedVarDef("foo", function_call),
      function_declaration);
}

TEST(ResolverTests, ResolvesMutualRecursionInGlobalScope) {
  const std::string source =
      "func ping() { pong(); }\n"
      "func pong() { ping(); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  const Parsing::UseResolver resolver =
      Parsing::BuildUseResolver(program, symbol_table);

  ASSERT_EQ(program.top_statements.size(), 2u);
  ASSERT_NE(program.top_statements[0], nullptr);
  ASSERT_NE(program.top_statements[1], nullptr);

  const auto* ping_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(ping_function, nullptr);
  ASSERT_NE(ping_function->body, nullptr);
  ASSERT_EQ(ping_function->body->statements.size(), 1u);
  ASSERT_NE(ping_function->body->statements[0], nullptr);

  const auto* pong_function =
      std::get_if<Parsing::FunctionDeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(pong_function, nullptr);
  ASSERT_NE(pong_function->body, nullptr);
  ASSERT_EQ(pong_function->body->statements.size(), 1u);
  ASSERT_NE(pong_function->body->statements[0], nullptr);

  const auto* ping_call_expression =
      std::get_if<Parsing::Expression>(&ping_function->body->statements[0]->value);
  ASSERT_NE(ping_call_expression, nullptr);
  const auto* ping_call =
      std::get_if<Parsing::FunctionCall>(&ping_call_expression->value);
  ASSERT_NE(ping_call, nullptr);

  const auto* pong_call_expression =
      std::get_if<Parsing::Expression>(&pong_function->body->statements[0]->value);
  ASSERT_NE(pong_call_expression, nullptr);
  const auto* pong_call =
      std::get_if<Parsing::FunctionCall>(&pong_call_expression->value);
  ASSERT_NE(pong_call, nullptr);

  EXPECT_EQ(resolver.GetUsedVarDef("pong", ping_call), pong_function);
  EXPECT_EQ(resolver.GetUsedVarDef("ping", pong_call), ping_function);
}

TEST(ResolverTests, ThrowsOnNestedBlockUseBeforeDefinitionInParentScope) {
  const std::string source =
      "func main() { { x = x + 10; } var x int = 0; print(x); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  EXPECT_THROW(
      Parsing::BuildUseResolver(program, symbol_table),
      std::runtime_error);
}

TEST(ResolverTests, ResolvesMethodCallReceiverVariable) {
  const std::string source =
      "class A { func ping() { } func ring() { } }\n"
      "var obj A;\n"
      "func main() { obj.ping(); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  const Parsing::UseResolver resolver =
      Parsing::BuildUseResolver(program, symbol_table);

  ASSERT_EQ(program.top_statements.size(), 3u);
  ASSERT_NE(program.top_statements[1], nullptr);
  ASSERT_NE(program.top_statements[2], nullptr);

  const auto* obj_declaration =
      std::get_if<Parsing::DeclarationStatement>(&program.top_statements[1]->value);
  ASSERT_NE(obj_declaration, nullptr);

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

  EXPECT_EQ(
      resolver.GetUsedVarDef("obj", &method_call->object_name),
      obj_declaration);
}

TEST(ResolverTests, ResolvesFieldAccessReceiverVariable) {
  const std::string source =
      "class A { var value int; func get(other A) int { return other.value; } }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  const Parsing::UseResolver resolver =
      Parsing::BuildUseResolver(program, symbol_table);

  ASSERT_EQ(program.top_statements.size(), 1u);
  ASSERT_NE(program.top_statements[0], nullptr);
  const auto* class_declaration =
      std::get_if<Parsing::ClassDeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(class_declaration, nullptr);
  ASSERT_EQ(class_declaration->methods.size(), 1u);
  ASSERT_EQ(class_declaration->methods[0].parameters.size(), 1u);
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

  EXPECT_EQ(
      resolver.GetUsedVarDef("other", &field_access->object_name),
      &class_declaration->methods[0].parameters[0]);
}

TEST(ResolverTests, ResolvesClassFieldUsedDirectlyInsideMethod) {
  const std::string source =
      "class A { var x int; func get() int { return x; } }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  const Parsing::UseResolver resolver =
      Parsing::BuildUseResolver(program, symbol_table);

  ASSERT_EQ(program.top_statements.size(), 1u);
  ASSERT_NE(program.top_statements[0], nullptr);

  const auto* class_declaration =
      std::get_if<Parsing::ClassDeclarationStatement>(&program.top_statements[0]->value);
  ASSERT_NE(class_declaration, nullptr);
  ASSERT_EQ(class_declaration->fields.size(), 1u);
  ASSERT_EQ(class_declaration->methods.size(), 1u);
  ASSERT_NE(class_declaration->methods[0].body, nullptr);
  ASSERT_EQ(class_declaration->methods[0].body->statements.size(), 1u);
  ASSERT_NE(class_declaration->methods[0].body->statements[0], nullptr);

  const auto* return_statement =
      std::get_if<Parsing::ReturnStatement>(&class_declaration->methods[0].body->statements[0]->value);
  ASSERT_NE(return_statement, nullptr);
  ASSERT_NE(return_statement->expr, nullptr);

  const auto* identifier =
      std::get_if<Parsing::IdentifierExpression>(&return_statement->expr->value);
  ASSERT_NE(identifier, nullptr);

  EXPECT_EQ(
      resolver.GetUsedVarDef("x", identifier),
      &class_declaration->fields[0]);
}

TEST(ResolverTests, ThrowsOnUnknownMethodForReceiverClass) {
  const std::string source =
      "class A { }\n"
      "var obj A;\n"
      "func main() { obj.missing(); }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  EXPECT_THROW(
      Parsing::BuildUseResolver(program, symbol_table),
      std::runtime_error);
}

TEST(ResolverTests, ThrowsOnUnknownFieldForReceiverClass) {
  const std::string source =
      "class A { var value int; func get(other A) int { return other.missing; } }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  EXPECT_THROW(
      Parsing::BuildUseResolver(program, symbol_table),
      std::runtime_error);
}

}  // namespace
