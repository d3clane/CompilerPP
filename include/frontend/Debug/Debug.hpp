#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace Parsing {

class ASTNode;

struct DebugInfo {
  std::string filename;
  std::pair<size_t, size_t> line_range{0, 0};
  std::pair<size_t, size_t> column_range{0, 0};
  std::pair<size_t, size_t> code_range{0, 0};
  std::vector<std::pair<size_t, size_t>> stressed_chars;

  friend bool operator==(const DebugInfo& left, const DebugInfo& right) = default;
};

class ASTDebugInfo {
 public:
  void SetInputCode(std::string input_code);
  const std::string& GetInputCode() const;

  void AddDebugInfo(const ASTNode* node, DebugInfo debug_info);
  const DebugInfo* GetDebugInfo(const ASTNode* node) const;

 private:
  std::string input_code_;
  std::map<const ASTNode*, DebugInfo> debug_info_by_node_;
};

DebugInfo CreateDebugInfo(
    std::string filename,
    std::pair<size_t, size_t> line_range,
    std::pair<size_t, size_t> column_range,
    std::pair<size_t, size_t> code_range = {0, 0},
    std::vector<std::pair<size_t, size_t>> stressed_chars = {});

}  // namespace Parsing
