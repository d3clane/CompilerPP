#include <string>
#include <stdexcept>

#include <gtest/gtest.h>

#include "Parsing/Parser.hpp"
#include "SemanticAnalysis/Resolver.hpp"
#include "SemanticAnalysis/SymbolTable.hpp"
#include "SemanticAnalysis/TypeChecker.hpp"
#include "SemanticAnalysis/TypeDefiner.hpp"
#include "TestParseUtils.hpp"

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
  const Parsing::TypeDefiner type_definer = Parsing::BuildTypeDefiner(program);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  const Parsing::UseResolver resolver =
      Parsing::BuildUseResolver(program, symbol_table);
  EXPECT_NO_THROW(Parsing::CheckTypes(program, resolver, type_definer));
}

void ExpectTypeCheckFail(const std::string& source) {
  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::TypeDefiner type_definer = Parsing::BuildTypeDefiner(program);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  EXPECT_THROW(
      {
        const Parsing::UseResolver resolver =
            Parsing::BuildUseResolver(program, symbol_table);
        Parsing::CheckTypes(program, resolver, type_definer);
      },
      std::runtime_error);
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

TEST(TypeCheckerTests, AcceptsAssigningDerivedClassToBaseClass) {
  const std::string source =
      "class Base { var id int; }\n"
      "class Derived:Base { var extra int; }\n"
      "var base Base;\n"
      "var derived Derived;\n"
      "func takes_base(value Base) { }\n"
      "func main() { var local_base Base = derived; base = derived; takes_base(derived); }\n";

  ExpectTypeCheckPass(source);
}

TEST(TypeCheckerTests, RejectsAssigningBaseClassToDerivedClass) {
  const std::string source =
      "class Base { var id int; }\n"
      "class Derived:Base { var extra int; }\n"
      "var base Base;\n"
      "var derived Derived;\n"
      "func main() { derived = base; }\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsAssigningUnrelatedClassTypes) {
  const std::string source =
      "class First { var id int; }\n"
      "class Second { var id int; }\n"
      "var first First;\n"
      "var second Second;\n"
      "func main() { first = second; }\n";

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

TEST(TypeCheckerTests, AcceptsClassAndArrayTypes) {
  const std::string source =
      "class Node { var value int; }\n"
      "class Derived:Node { var next Node; }\n"
      "var root Node;\n"
      "var storage int[];\n"
      "func main() { var local Node = root; var local_arr int[] = storage; }\n";

  ExpectTypeCheckPass(source);
}

TEST(TypeCheckerTests, RejectsUnknownClassTypeInDeclaration) {
  const std::string source = "var x Unknown;\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsUnknownBaseClass) {
  const std::string source = "class Derived:MissingBase { }\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, AcceptsMethodCallAndInheritedMethodCall) {
  const std::string source =
      "class Base { func id(v int) int { return v; } }\n"
      "class Derived:Base { func twice(v int) int { return v + v; } }\n"
      "var obj Derived;\n"
      "func main() { var a int = obj.id(1); var b int = obj.twice(2); print(a + b); }\n";

  ExpectTypeCheckPass(source);
}

TEST(TypeCheckerTests, RejectsMethodCallOnNonClassValue) {
  const std::string source =
      "var x int = 1;\n"
      "func main() { x.foo(); }\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsUnknownMethodCall) {
  const std::string source =
      "class Node { var value int; }\n"
      "var node Node;\n"
      "func main() { node.missing(); }\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, AcceptsFieldAccessForSameClassInsideMethod) {
  const std::string source =
      "class Node { var value int; func read(other Node) int { return other.value; } }\n"
      "var node Node;\n"
      "func main() { var x int = node.read(node); }\n";

  ExpectTypeCheckPass(source);
}

TEST(TypeCheckerTests, RejectsFieldAccessOutsideMethod) {
  const std::string source =
      "class Node { var value int; }\n"
      "var node Node;\n"
      "var x int = node.value;\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsFieldAccessForDifferentClassTypeInsideMethod) {
  const std::string source =
      "class B { var y int; }\n"
      "class A { var x int; func bad(other B) int { return other.y; } }\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsUnknownFieldForCurrentClassType) {
  const std::string source =
      "class A { var x int; func bad(other A) int { return other.y; } }\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsVariableRedefinitionInSameScope) {
  const std::string source =
      "func main() { var x int = 1; var x int = 2; }\n";

  const Parsing::Program program = Parsing::ParseSource(source);
  EXPECT_ANY_THROW(Parsing::BuildSymbolTable(program));
}

TEST(TypeCheckerTests, AcceptsUsingGlobalClassTypeBeforeDefinition) {
  const std::string source =
      "var head Node;\n"
      "class Node { var value int; }\n";

  ExpectTypeCheckPass(source);
}

TEST(TypeCheckerTests, AcceptsUsingLocalClassTypeAfterDefinition) {
  const std::string source =
      "func main() { class Local { var value int; } var x Local; }\n";

  ExpectTypeCheckPass(source);
}

TEST(TypeCheckerTests, RejectsUsingLocalClassTypeBeforeDefinition) {
  const std::string source =
      "func main() { var x Local; class Local { var value int; } }\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsArithmeticWithClassTypeReturnedFromFunction) {
  const std::string source =
      "class Node { var value int; }\n"
      "var node Node;\n"
      "func make() Node { return node; }\n"
      "func main() { var x int = make() + 1; }\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsArithmeticBetweenBoolAndIntMethodCalls) {
  const std::string source =
      "class Ops {\n"
      "  func as_bool() bool { return true; }\n"
      "  func as_int() int { return 1; }\n"
      "  func bad() int { return as_bool() + as_int(); }\n"
      "}\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsArithmeticBetweenBoolFieldAndIntMethodCall) {
  const std::string source =
      "class Box {\n"
      "  var flag bool;\n"
      "  func value() int { return 1; }\n"
      "  func bad() int { return flag + value(); }\n"
      "}\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, RejectsUsingVoidFunctionInArithmeticExpression) {
  const std::string source =
      "func noop() { }\n"
      "func main() { var x int = noop() + 1; }\n";

  ExpectTypeCheckFail(source);
}

TEST(TypeCheckerTests, AcceptsComplexNestedShadowingWithAssignmentBeforeInnerDeclaration) {
  const std::string source =
      "func main() { "
      "var x int = 0; "
      "{ "
      "{ x = x + 10; } "
      "var x int = 5; "
      "print(x); "
      "} "
      "print(x); "
      "}\n";

  ExpectTypeCheckPass(source);
}

}  // namespace
