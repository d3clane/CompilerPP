cmake_minimum_required(VERSION 3.20)

project(CompilerPP LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(BISON 3.8 REQUIRED)
find_package(FLEX REQUIRED)

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")

BISON_TARGET(
  CompilerParser
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Parsing/Parser.y"
  "${GENERATED_DIR}/BisonParser.cpp"
  DEFINES_FILE "${GENERATED_DIR}/BisonParser.hpp"
)

FLEX_TARGET(
  CompilerLexer
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Tokenizing/Lexer.l"
  "${GENERATED_DIR}/FlexLexer.cpp"
)

add_library(
  compiler_frontend
  ${BISON_CompilerParser_OUTPUTS}
  ${FLEX_CompilerLexer_OUTPUTS}
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Parsing/Parser.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Visitors/Printer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Visitors/PrinterAST.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Visitors/Interpreter.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/SemanticAnalysis/SymbolTable.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/SemanticAnalysis/Resolver.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/SemanticAnalysis/TypeChecker.cpp"
)

target_include_directories(
  compiler_frontend
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include/frontend"
    "${GENERATED_DIR}"
)

target_compile_features(compiler_frontend PUBLIC cxx_std_20)

add_executable(
  compiler_frontend_cli
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/main.cpp"
)

target_link_libraries(
  compiler_frontend_cli
  PRIVATE
    compiler_frontend
)

target_compile_features(compiler_frontend_cli PRIVATE cxx_std_20)

enable_testing()

find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(
  compiler_frontend_tests
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/ParserTreeTests.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/InterpreterTests.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/SymbolTableTests.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/ResolverTests.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/TypeCheckerTests.cpp"
)

target_link_libraries(
  compiler_frontend_tests
  PRIVATE
    compiler_frontend
    GTest::gtest_main
)

target_compile_features(compiler_frontend_tests PRIVATE cxx_std_20)

gtest_discover_tests(compiler_frontend_tests)
