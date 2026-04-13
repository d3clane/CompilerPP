#pragma once

#include <string>

#include "Debug/Debug.hpp"
#include "Debug/Errors.hpp"

namespace Front {

class DebugCtx {
 public:
  explicit DebugCtx(std::string filename = "<input>");

  const std::string& GetFilename() const;

  ASTDebugInfo& GetAstDebugInfo();
  const ASTDebugInfo& GetAstDebugInfo() const;

  FrontendErrors& GetErrors();
  const FrontendErrors& GetErrors() const;

  void SetInputCode(std::string input_code);

 private:
  std::string filename_;
  ASTDebugInfo ast_debug_info_;
  FrontendErrors frontend_errors_;
};

}  // namespace Front
