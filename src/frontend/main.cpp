#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "Visitors/Interpreter.hpp"
#include "Parsing/Parser.hpp"

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <input-file>\n";
    return 1;
  }

  const std::string input_path = argv[1];
  std::ifstream input_stream(input_path);
  if (!input_stream) {
    std::cerr << "Failed to open input file: " << input_path << "\n";
    return 1;
  }

  const std::string source{
      std::istreambuf_iterator<char>(input_stream),
      std::istreambuf_iterator<char>()};

  try {
    const Parsing::Program program = Parsing::ParseSource(source);
    Parsing::Interpret(program, std::cout);
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << "\n";
    return 1;
  }

  return 0;
}
