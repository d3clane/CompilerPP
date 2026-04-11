cmake_minimum_required(VERSION 3.20)

project(CompilerPP LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(BISON 3.8 REQUIRED)
find_package(FLEX REQUIRED)
if(NOT DEFINED LLVM_CONFIG_EXE)
  foreach(LLVM_MAJOR_CANDIDATE 21 20 19)
    set(LLVM_CONFIG_CANDIDATE "/opt/homebrew/opt/llvm@${LLVM_MAJOR_CANDIDATE}/bin/llvm-config")
    if(EXISTS "${LLVM_CONFIG_CANDIDATE}" AND EXISTS "/opt/homebrew/opt/lld@${LLVM_MAJOR_CANDIDATE}/lib/cmake/lld/LLDConfig.cmake")
      set(LLVM_CONFIG_EXE "${LLVM_CONFIG_CANDIDATE}" CACHE FILEPATH "Path to llvm-config")
      break()
    endif()
  endforeach()
endif()

find_program(LLVM_CONFIG_EXE NAMES llvm-config REQUIRED)
execute_process(
  COMMAND "${LLVM_CONFIG_EXE}" --includedir
  OUTPUT_VARIABLE LLVM_INCLUDE_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
  COMMAND "${LLVM_CONFIG_EXE}" --prefix
  OUTPUT_VARIABLE LLVM_PREFIX
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
  COMMAND "${LLVM_CONFIG_EXE}" --version
  OUTPUT_VARIABLE LLVM_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REGEX MATCH "^[0-9]+" LLVM_VERSION_MAJOR "${LLVM_VERSION}")
set(LLVM_DIR "${LLVM_PREFIX}/lib/cmake/llvm" CACHE PATH "LLVM CMake package directory" FORCE)
execute_process(
  COMMAND "${LLVM_CONFIG_EXE}" --libfiles core nativecodegen target targetparser mc mcparser asmparser asmprinter passes support
  OUTPUT_VARIABLE LLVM_LIBS
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
  COMMAND "${LLVM_CONFIG_EXE}" --system-libs
  OUTPUT_VARIABLE LLVM_SYSTEM_LIBS
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
separate_arguments(LLVM_LIBS UNIX_COMMAND "${LLVM_LIBS}")
separate_arguments(LLVM_SYSTEM_LIBS UNIX_COMMAND "${LLVM_SYSTEM_LIBS}")

find_package(
  LLD ${LLVM_VERSION} EXACT CONFIG REQUIRED
  HINTS
    "${LLVM_PREFIX}"
    "/opt/homebrew/opt/lld@${LLVM_VERSION_MAJOR}"
)

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
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Debug/DebugCtx.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Debug/Debug.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Debug/Errors.cpp"
  ${BISON_CompilerParser_OUTPUTS}
  ${FLEX_CompilerLexer_OUTPUTS}
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Parsing/Parser.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Parsing/Types.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Lowering/ExecutableLowering.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Lowering/LLVMConstructUtils.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Lowering/LLVMIRLowering.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Lowering/ObjectFileLowering.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Utils/ClassMemberLookup.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Visitors/Printer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Visitors/PrinterAST.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/Visitors/Interpreter.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/SemanticAnalysis/StatementNumerizer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/SemanticAnalysis/ClassRelations.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/SemanticAnalysis/FunctionRelations.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/SemanticAnalysis/GlobalRelations.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/SemanticAnalysis/SymbolTable.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/SemanticAnalysis/TypeDefiner.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/SemanticAnalysis/Resolver.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/SemanticAnalysis/AccessAllowanceChecker.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/SemanticAnalysis/TypeChecker.cpp"
)

target_include_directories(
  compiler_frontend
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include/frontend"
    "${GENERATED_DIR}"
  PRIVATE
    "${LLVM_INCLUDE_DIR}"
    "${LLD_INCLUDE_DIRS}"
)

target_link_libraries(
  compiler_frontend
  PRIVATE
    ${LLVM_LIBS}
    ${LLVM_SYSTEM_LIBS}
    lldMachO
    lldCommon
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
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/LoweringTests.cpp"
)

target_link_libraries(
  compiler_frontend_tests
  PRIVATE
    compiler_frontend
    GTest::gtest_main
)

target_compile_features(compiler_frontend_tests PRIVATE cxx_std_20)

gtest_discover_tests(compiler_frontend_tests)
