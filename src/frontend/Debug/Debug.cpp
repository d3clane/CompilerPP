#include "Debug/Debug.hpp"

#include <utility>

namespace Front {

void ASTDebugInfo::SetInputCode(std::string input_code) {
  input_code_ = std::move(input_code);
}

const std::string& ASTDebugInfo::GetInputCode() const {
  return input_code_;
}

void ASTDebugInfo::AddDebugInfo(const ASTNode* node, DebugInfo debug_info) {
  if (node == nullptr) {
    return;
  }

  debug_info_by_node_[node] = std::move(debug_info);
}

const DebugInfo* ASTDebugInfo::GetDebugInfo(const ASTNode* node) const {
  if (node == nullptr) {
    return nullptr;
  }

  const auto info_it = debug_info_by_node_.find(node);
  if (info_it == debug_info_by_node_.end()) {
    return nullptr;
  }

  return &info_it->second;
}

DebugInfo CreateDebugInfo(
    std::string filename,
    std::pair<size_t, size_t> line_range,
    std::pair<size_t, size_t> column_range,
    std::pair<size_t, size_t> code_range,
    std::vector<std::pair<size_t, size_t>> stressed_chars) {
  return DebugInfo{
      std::move(filename),
      std::move(line_range),
      std::move(column_range),
      std::move(code_range),
      std::move(stressed_chars)};
}

}  // namespace Front
