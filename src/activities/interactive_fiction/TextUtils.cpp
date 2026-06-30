#include "TextUtils.h"
#include <algorithm>

TextUtils::WrappedText TextUtils::WrapText(const std::string& text, int maxCharsPerLine) {
  WrappedText result;
  std::string line;

  for (char c : text) {
    if (c == '\n') {
      result.lines.push_back(line);
      line.clear();
    } else if (static_cast<int>(line.length()) >= maxCharsPerLine) {
      result.lines.push_back(line);
      line.clear();
      line += c;
    } else {
      line += c;
    }
  }

  if (!line.empty()) {
    result.lines.push_back(line);
  }

  result.totalLines = result.lines.size();
  return result;
}

std::vector<std::string> TextUtils::GetVisibleLines(
    const WrappedText& wrapped,
    int maxVisibleLines,
    int scrollOffset
) {
  std::vector<std::string> visible;
  for (int i = scrollOffset; i < scrollOffset + maxVisibleLines && i < wrapped.totalLines; ++i) {
    if (i >= 0 && i < static_cast<int>(wrapped.lines.size())) {
      visible.push_back(wrapped.lines[i]);
    }
  }
  return visible;
}
