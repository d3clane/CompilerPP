#include "Parsing/Types.hpp"

#include <cstddef>

#include "Parsing/Ast.hpp"

namespace Parsing {

FuncType BuildFunctionType(const FunctionDeclarationStatement& function_declaration) {
  FuncType function_type;
  for (size_t i = 0; i < function_declaration.parameters.size(); ++i) {
    function_type.parameter_types.push_back(function_declaration.parameters[i].type);
  }

  if (function_declaration.return_type.has_value()) {
    function_type.return_type =
        std::make_unique<Type>(*function_declaration.return_type);
  }

  return function_type;
}

}  // namespace Parsing
