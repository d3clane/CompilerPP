#include <filesystem>
#include <regex>
#include <string>

#include <gtest/gtest.h>

#include "Lowering/ExecutableLowering.hpp"
#include "Lowering/LLVMIRLowering.hpp"
#include "Lowering/ObjectFileLowering.hpp"
#include "SemanticAnalysis/AccessAllowanceChecker.hpp"
#include "SemanticAnalysis/Resolver.hpp"
#include "SemanticAnalysis/SymbolTable.hpp"
#include "SemanticAnalysis/TypeChecker.hpp"
#include "TestParseUtils.hpp"

namespace {

std::string LowerSource(const std::string& source) {
  const Front::Program program = Front::ParseSource(source);
  Front::SymbolTable symbol_table = Front::BuildSymbolTable(program);
  const Front::UseResolver resolver =
      Front::BuildUseResolver(program, symbol_table);
  Front::CheckAccessAllowance(program, resolver);
  Front::CheckTypes(program, resolver);
  return Front::LowerToLLVMIR(program, resolver);
}

void LowerSourceToObjectFile(
    const std::string& source,
    const std::filesystem::path& output_path) {
  const Front::Program program = Front::ParseSource(source);
  Front::SymbolTable symbol_table = Front::BuildSymbolTable(program);
  const Front::UseResolver resolver =
      Front::BuildUseResolver(program, symbol_table);
  Front::CheckAccessAllowance(program, resolver);
  Front::CheckTypes(program, resolver);
  Front::LLVMIRModule llvm_ir =
      Front::LowerToLLVMIRModule(program, resolver);
  Back::LowerToObjectFile(llvm_ir.GetModule(), output_path.string());
}

void LowerSourceToExecutableFile(
    const std::string& source,
    const std::filesystem::path& output_path) {
  const Front::Program program = Front::ParseSource(source);
  Front::SymbolTable symbol_table = Front::BuildSymbolTable(program);
  const Front::UseResolver resolver =
      Front::BuildUseResolver(program, symbol_table);
  Front::CheckAccessAllowance(program, resolver);
  Front::CheckTypes(program, resolver);
  Front::LLVMIRModule llvm_ir =
      Front::LowerToLLVMIRModule(program, resolver);
  Back::LowerToExecutableFile(llvm_ir.GetModule(), output_path.string());
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

TEST(LoweringTests, EmitsNativeObjectFile) {
  const std::string source =
      "func main() int { return 0; }\n";
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() / "compilerpp_lowering_test.o";
  std::filesystem::remove(output_path);

  LowerSourceToObjectFile(source, output_path);

  ASSERT_TRUE(std::filesystem::exists(output_path));
  EXPECT_GT(std::filesystem::file_size(output_path), 0u);
  std::filesystem::remove(output_path);
}

TEST(LoweringTests, EmitsNativeExecutableFile) {
  const std::string source =
      "func main() int { return 0; }\n";
  const std::filesystem::path output_path =
      std::filesystem::temp_directory_path() / "compilerpp_lowering_test";
  std::filesystem::remove(output_path);

  LowerSourceToExecutableFile(source, output_path);

  ASSERT_TRUE(std::filesystem::exists(output_path));
  EXPECT_GT(std::filesystem::file_size(output_path), 0u);
  std::filesystem::remove(output_path);
}

}  // namespace
