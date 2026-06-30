#pragma once

#include <string>
#include <vector>

class TextUtils {
 public:
  struct WrappedText {
    std::vector<std::string> lines;
    int totalLines = 0;
  };

  // Wrap text to fit within maxCharsPerLine
  static WrappedText WrapText(const std::string& text, int maxCharsPerLine);

  // Get visible portion given scroll offset
  static std::vector<std::string> GetVisibleLines(
      const WrappedText& wrapped,
      int maxVisibleLines,
      int scrollOffset
  );
};
