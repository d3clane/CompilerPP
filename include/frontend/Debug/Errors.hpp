#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Debug/Debug.hpp"

namespace Front {

class FrontendErrors {
 public:
  struct OneError {
    DebugInfo debug_info;
    std::string error_message;

    friend bool operator==(const OneError& left, const OneError& right) = default;
  };

  explicit FrontendErrors(const ASTDebugInfo& ast_debug_info);

  void AddError(const ASTNode* node, const std::string& msg);
  void AddError(const DebugInfo& debug_info, const std::string& msg);
  void SetLimit(size_t limit);

  std::string GetErrors() const;
  [[noreturn]] void ThrowErrors() const;
  bool HasErrors() const;

 private:
  const ASTDebugInfo& ast_debug_info_;
  std::vector<OneError> errors_;
  size_t limit_ = static_cast<size_t>(-1);
};

}  // namespace Front
