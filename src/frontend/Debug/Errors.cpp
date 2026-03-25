#include "Debug/Errors.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace Parsing {

namespace {

std::string GetContextText(const ASTDebugInfo& ast_debug_info, const DebugInfo& debug_info) {
  const std::string& input_code = ast_debug_info.GetInputCode();
  if (input_code.empty()) {
    return std::string();
  }

  const size_t range_begin = std::min(debug_info.code_range.first, input_code.size());
  const size_t range_end = std::min(debug_info.code_range.second, input_code.size());
  if (range_begin >= range_end) {
    return std::string();
  }

  return input_code.substr(range_begin, range_end - range_begin);
}

std::vector<std::pair<size_t, size_t>> BuildStressRanges(const DebugInfo& debug_info) {
  if (!debug_info.stressed_chars.empty()) {
    return debug_info.stressed_chars;
  }

  if (debug_info.code_range.second <= debug_info.code_range.first) {
    return {};
  }

  const size_t context_size = debug_info.code_range.second - debug_info.code_range.first;
  return {{0, context_size}};
}

std::string BuildStressLine(
    const std::string& context_text,
    const DebugInfo& debug_info) {
  if (context_text.empty()) {
    return std::string();
  }

  std::string stress_line(context_text.size(), ' ');
  const std::vector<std::pair<size_t, size_t>> stress_ranges =
      BuildStressRanges(debug_info);
  for (size_t i = 0; i < stress_ranges.size(); ++i) {
    const size_t range_begin = stress_ranges[i].first;
    const size_t range_end = stress_ranges[i].second;
    if (range_begin >= range_end || range_begin >= stress_line.size()) {
      continue;
    }

    const size_t clamped_end = std::min(range_end, stress_line.size());
    for (size_t pos = range_begin; pos < clamped_end; ++pos) {
      stress_line[pos] = '^';
    }
  }

  return stress_line;
}

}  // namespace

FrontendErrors::FrontendErrors(const ASTDebugInfo& ast_debug_info)
    : ast_debug_info_(ast_debug_info) {}

void FrontendErrors::AddError(const ASTNode* node, const std::string& msg) {
  if (node == nullptr) {
    AddError(DebugInfo{}, msg);
    return;
  }

  const DebugInfo* debug_info = ast_debug_info_.GetDebugInfo(node);
  if (debug_info == nullptr) {
    AddError(DebugInfo{}, msg);
    return;
  }

  AddError(*debug_info, msg);
}

void FrontendErrors::AddError(const DebugInfo& debug_info, const std::string& msg) {
  errors_.push_back(OneError{debug_info, msg});
  if (errors_.size() >= limit_) {
    ThrowErrors();
  }
}

void FrontendErrors::SetLimit(size_t limit) {
  limit_ = limit;
}

std::string FrontendErrors::GetErrors() const {
  std::ostringstream stream;
  for (size_t i = 0; i < errors_.size(); ++i) {
    const OneError& error = errors_[i];
    const DebugInfo& debug_info = error.debug_info;

    stream << "Error: " << error.error_message;
    if (!debug_info.filename.empty()) {
      stream << " [" << debug_info.filename;
      if (debug_info.line_range.first != 0 && debug_info.column_range.first != 0) {
        stream << ":" << debug_info.line_range.first << ":" << debug_info.column_range.first;
      }
      stream << "]";
    }
    stream << "\n";

    const std::string context_text = GetContextText(ast_debug_info_, debug_info);
    if (!context_text.empty()) {
      stream << context_text << "\n";
      const std::string stress_line = BuildStressLine(context_text, debug_info);
      if (!stress_line.empty()) {
        stream << stress_line << "\n";
      }
    }

    if (i + 1 < errors_.size()) {
      stream << "\n";
    }
  }

  return stream.str();
}

[[noreturn]] void FrontendErrors::ThrowErrors() const {
  throw std::runtime_error(GetErrors());
}

bool FrontendErrors::HasErrors() const {
  return !errors_.empty();
}

}  // namespace Parsing
