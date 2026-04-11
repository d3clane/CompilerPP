#include <regex>
#include <string>

#include <gtest/gtest.h>

#include "Lowering/LLVMIRLowering.hpp"
#include "SemanticAnalysis/AccessAllowanceChecker.hpp"
#include "SemanticAnalysis/Resolver.hpp"
#include "SemanticAnalysis/SymbolTable.hpp"
#include "SemanticAnalysis/TypeChecker.hpp"
#include "SemanticAnalysis/TypeDefiner.hpp"
#include "TestParseUtils.hpp"

namespace {

std::string LowerSource(const std::string& source) {
  const Parsing::Program program = Parsing::ParseSource(source);
  const Parsing::TypeDefiner type_definer = Parsing::BuildTypeDefiner(program);
  Parsing::SymbolTable symbol_table = Parsing::BuildSymbolTable(program);
  const Parsing::UseResolver resolver =
      Parsing::BuildUseResolver(program, symbol_table);
  Parsing::CheckAccessAllowance(program, resolver, type_definer);
  Parsing::CheckTypes(program, resolver, type_definer);
  return Parsing::LowerToLLVMIR(program, resolver, type_definer);
}

TEST(LoweringTests, LowersDerivedLayoutWithEmbeddedBaseAndAllocator) {
  const std::string source =
      "class Base { var id int = 1; }\n"
      "class Derived:Base { var extra int = 2; }\n"
      "func main() int { var obj Derived; return 0; }\n";

  const std::string ir = LowerSource(source);

  EXPECT_NE(
      ir.find("%class__Base = type { ptr, i32 }"),
      std::string::npos);
  EXPECT_NE(
      ir.find("%class__Derived = type { %class__Base, i32 }"),
      std::string::npos);
  EXPECT_NE(
      ir.find("define ptr @new__Derived() {"),
      std::string::npos);
  EXPECT_NE(
      ir.find("store ptr @vtable__Derived, ptr"),
      std::string::npos);
  EXPECT_NE(
      ir.find("call ptr @new__Derived()"),
      std::string::npos);
}

TEST(LoweringTests, LowersImplicitMethodCallsThroughVTable) {
  const std::string source =
      "class Base { "
      "func Value() int { return 1; } "
      "func Forward() int { return Value(); } "
      "}\n"
      "func main() int { var obj Base; return obj.Forward(); }\n";

  const std::string ir = LowerSource(source);

  EXPECT_NE(
      ir.find("@vtable__Base = internal"),
      std::string::npos);
  EXPECT_NE(
      ir.find("[2 x ptr] [ptr @__Base__Value, ptr @__Base__Forward]"),
      std::string::npos);
  EXPECT_NE(
      ir.find("define i32 @__Base__Value(ptr %__this) {"),
      std::string::npos);
  EXPECT_EQ(
      ir.find("%__tmp"),
      std::string::npos);
  EXPECT_EQ(
      ir.find("call i32 @__Base__Value__1_1(ptr"),
      std::string::npos);
  EXPECT_TRUE(std::regex_search(
      ir,
      std::regex(
          R"(getelementptr inbounds( nuw)? \[2 x ptr\], ptr %[A-Za-z0-9_]+, i32 0, i32 0)")));
  EXPECT_TRUE(std::regex_search(
      ir,
      std::regex(R"(call i32 %[A-Za-z0-9_]+\(ptr %[A-Za-z0-9_]+\))")));
}

TEST(LoweringTests, LowersDerivedToBaseArgumentUpcastWithPointerAdjustment) {
  const std::string source =
      "class Base { func Value() int { return 1; } }\n"
      "class Derived:Base { func Value() int { return 2; } }\n"
      "func Take(value Base) int { return value.Value(); }\n"
      "func main() int { var obj Derived; return Take(obj); }\n";

  const std::string ir = LowerSource(source);

  EXPECT_TRUE(std::regex_search(
      ir,
      std::regex(
          R"(getelementptr inbounds( nuw)? %class__Derived, ptr %[A-Za-z0-9_]+, i32 0, i32 0)")));
  EXPECT_TRUE(std::regex_search(
      ir,
      std::regex(R"(define i32 @Take\(ptr %\d+\) \{)")));
  EXPECT_TRUE(std::regex_search(
      ir,
      std::regex(
          R"(getelementptr inbounds( nuw)? \[1 x ptr\], ptr %[A-Za-z0-9_]+, i32 0, i32 0)")));
}

TEST(LoweringTests, LowersGlobalClassInitializationIntoGlobalInitFunction) {
  const std::string source =
      "class Node { var value int = 7; }\n"
      "var root Node;\n"
      "func main() { }\n";

  const std::string ir = LowerSource(source);

  EXPECT_NE(
      ir.find("@root = internal global ptr null"),
      std::string::npos);
  EXPECT_NE(
      ir.find("define internal void @__global_init() {"),
      std::string::npos);
  EXPECT_NE(
      ir.find("call ptr @new__Node()"),
      std::string::npos);
  EXPECT_TRUE(std::regex_search(
      ir,
      std::regex(R"(store ptr %[A-Za-z0-9_]+, ptr @root)")));
  EXPECT_NE(
      ir.find("call void @__global_init()"),
      std::string::npos);
}

TEST(LoweringTests, LowersDeleteStatementThroughClassDeallocator) {
  const std::string source =
      "class Node { var value int = 7; }\n"
      "func main() { var node Node; delete node; }\n";

  const std::string ir = LowerSource(source);

  EXPECT_NE(
      ir.find("declare void @free(ptr)"),
      std::string::npos);
  EXPECT_NE(
      ir.find("define void @delete__Node(ptr %__object) {"),
      std::string::npos);
  EXPECT_NE(
      ir.find("call void @free(ptr %__object)"),
      std::string::npos);
  EXPECT_TRUE(std::regex_search(
      ir,
      std::regex(R"(call void @delete__Node\(ptr %[A-Za-z0-9_]+\))")));
}

TEST(LoweringTests, KeepsNestedFunctionMangledWhileGlobalFunctionStaysPlain) {
  const std::string source =
      "func Foo() int { return 1; }\n"
      "func main() int {\n"
      "  func Helper() int { return Foo(); }\n"
      "  return Helper();\n"
      "}\n";

  const std::string ir = LowerSource(source);

  EXPECT_NE(
      ir.find("define i32 @Foo() {"),
      std::string::npos);
  EXPECT_TRUE(std::regex_search(
      ir,
      std::regex(R"(define i32 @Helper__[0-9_]+\(\))")));
  EXPECT_TRUE(std::regex_search(
      ir,
      std::regex(R"(call i32 @Helper__[0-9_]+\(\))")));
}

}  // namespace
