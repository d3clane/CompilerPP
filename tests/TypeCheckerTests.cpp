#include <string>
#include <stdexcept>

#include <gtest/gtest.h>

#include "Parsing/Parser.hpp"
#include "SemanticAnalysis/Resolver.hpp"
#include "SemanticAnalysis/SymbolTable.hpp"
#include "SemanticAnalysis/TypeChecker.hpp"

namespace {

static_assert(
    Parsing::IsSupportedExprOp<Parsing::AddExpression, Parsing::IntType>());
static_assert(
    !Parsing::IsSupportedExprOp<Parsing::AddExpression, Parsing::BoolType>());
static_assert(
    Parsing::IsSupportedExprOp<Parsing::UnaryNotExpression, Parsing::BoolType>());
static_assert(
    !Parsing::IsSupportedExprOp<Parsing::UnaryNotExpression, Parsing::IntType>());

void ExpectTypeCheckPass(const std::string& source) {
  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  const Parsing::UseResolver resolver =
      Parsing::BuildUseResolver(program, symbol_table);
  EXPECT_NO_THROW(Parsing::CheckTypes(program, resolver, symbol_table));
}

void ExpectTypeCheckFail(const std::string& source) {
  const Parsing::Program program = Parsing::ParseSource(source);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  const Parsing::UseResolver resolver =
      Parsing::BuildUseResolver(program, symbol_table);
  EXPECT_THROW(Parsing::CheckTypes(program, resolver, symbol_table), std::runtime_error);
}

TEST(TypeCheckerTests, AcceptsWellTypedProgram) {
  const std::string source =
      "func sum(a int, b int) int { return a + b; }\n"
      "var flag bool = true;\n"
      "var x int = 1;\n"
      "func main() { var y int = sum(x, 2); if flag && y > 0 { print(y); } else { print(0); } sum(a: y, b: 3); }\n";

  ExpectTypeCheckPass(source);
}

TEST(TypeCheckerTests, RejectsDeclarationInitializerTypeMismatch) {
  const std::string source = "var x int = true;\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsUnsupportedBoolArithmetic) {
  const std::string source = "var x bool = true + false;\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsUnsupportedBoolRelationalComparison) {
  const std::string source = "var x bool = true < false;\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsNonBoolIfCondition) {
  const std::string source = "func main() { if 1 { } }\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsAssignmentTypeMismatch) {
  const std::string source =
      "var x int;\n"
      "func main() { x = true; }\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsReturnTypeMismatch) {
  const std::string source = "func foo() int { return true; }\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsReturningValueFromVoidFunction) {
  const std::string source = "func foo() { return 1; }\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsUnaryNotOnInt) {
  const std::string source = "var x int = !1;\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsFunctionCallArgumentTypeMismatch) {
  const std::string source =
      "func foo(a int, b bool) int { return a; }\n"
      "func main() { foo(1, 2); }\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsNamedFunctionCallArgumentTypeMismatch) {
  const std::string source =
      "func foo(a int, b bool) int { return a; }\n"
      "func main() { foo(a: 1, b: 2); }\n";

  ExpectTypeCheckFail(source);
}

}  // namespace
