#include "Debug/DebugCtx.hpp"

#include <utility>

namespace Parsing {

DebugCtx::DebugCtx(std::string filename)
    : filename_(std::move(filename)),
      frontend_errors_(ast_debug_info_) {}

const std::string& DebugCtx::GetFilename() const {
  return filename_;
}

ASTDebugInfo& DebugCtx::GetAstDebugInfo() {
  return ast_debug_info_;
}

const ASTDebugInfo& DebugCtx::GetAstDebugInfo() const {
  return ast_debug_info_;
}

FrontendErrors& DebugCtx::GetErrors() {
  return frontend_errors_;
}

const FrontendErrors& DebugCtx::GetErrors() const {
  return frontend_errors_;
}

void DebugCtx::SetInputCode(std::string input_code) {
  ast_debug_info_.SetInputCode(std::move(input_code));
}

}  // namespace Parsing
