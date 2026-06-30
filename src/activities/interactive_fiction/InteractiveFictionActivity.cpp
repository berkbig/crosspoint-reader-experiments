#include "InteractiveFictionActivity.h"
#include "TextUtils.h"
#include "Logging.h"
#include "HalDisplay.h"
#include <algorithm>

namespace {
constexpr int kTopPaddingPx = 46;
constexpr int kBottomPaddingPx = 72;
constexpr int kLineHeightPx = 22;
constexpr int kTextInsetPx = 10;
constexpr int kMinCharsPerLine = 18;
constexpr int kMaxCharsPerLine = 36;
constexpr int kAverageCharWidthPx = 10;

int GetWrapWidthChars(int screenWidth) {
  const int availableWidth = std::max(1, screenWidth - (2 * kTextInsetPx));
  const int estimatedChars = availableWidth / kAverageCharWidthPx;
  return std::clamp(estimatedChars, kMinCharsPerLine, kMaxCharsPerLine);
}

int GetVisibleLineCount(int screenHeight) {
  const int contentTopY = kTopPaddingPx;
  const int contentBottomY = std::max(kTopPaddingPx + kLineHeightPx, screenHeight - kBottomPaddingPx);
  const int contentHeight = std::max(1, contentBottomY - contentTopY);
  return std::max(1, contentHeight / kLineHeightPx);
}
}

void InteractiveFictionActivity::generateTestBlocks() {
  textBlocks = {
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore "
      "et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
      "aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum "
      "dolore eu fugiat nulla pariatur excepteur sint occaecat cupidatat non proident."
      "Sed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium doloremque laudantium."
      "Totam rem aperiam eaque ipsa quae ab illo inventore veritatis et quasi architecto beatae vitae dicta "
      "sunt explicabo. Nemo enim ipsam voluptatem quia voluptas sit aspernatur aut odit aut fugit.",

      "A long time ago in a quiet library there lived a small brass key that had forgotten its purpose. "
      "Every evening the key sat beside a drawer of old letters and listened to the whisper of pages turning. "
      "One night a young reader arrived with a lantern and a question, and the key felt the first spark of meaning "
      "return to its metal teeth. The reader opened the drawer, found the hidden note, and the key finally knew "
      "it had been waiting all along for a hand to turn it."
      "The letters were not treasure but memory, and the memory was bright enough to light the room."
      "When the morning came the key rested in the lock, patient and warm, as if it had always belonged there.",

      "The river carried the city in pieces, a rooftop here, a lamp post there, a bridge made of old names. "
      "At the edge of the water stood a clockmaker who repaired not clocks but lost minutes, coaxing them back "
      "into the day one by one. He worked quietly until the sound of distant bells made him look up, and he saw "
      "three children waiting with a pocket watch that had stopped at the exact second the moon changed shape. "
      "The clockmaker smiled because he had been expecting them, and because some stories begin with a small pause."
      "He opened the watch, found the missing second, and handed it back to the children as if it were a pearl.",

      "Under the blue awning of the market square, a thin musician played a tune that made the air feel lighter. "
      "Near the fountain a baker folded dough into crescents while a child counted the coins in a tin cup. "
      "Every voice in the square seemed to belong to a different weather system, one to rain, one to sunlight, "
      "one to the warm smell of cedar and tea. When the musician finished, the crowd did not clap at once. "
      "They listened a little longer, as if the music had taught them how to breathe more deeply."
      "By evening the square was full of shadows and laughter, and the tune still lingered in the corners of every doorway."
  };
}

void InteractiveFictionActivity::onEnter() {
  LOG_INF("IF", "InteractiveFictionActivity::onEnter");
  generateTestBlocks();
  currentBlockIndex = 0;
  scrollOffset = 0;
  updateScrollBounds();
  requestUpdate();
}

void InteractiveFictionActivity::onExit() {
  LOG_INF("IF", "InteractiveFictionActivity::onExit");
}

void InteractiveFictionActivity::loop() {
  const uint32_t now = millis();
  if (now - lastButtonPressTime < DEBOUNCE_MS) {
    return;
  }

  if (mappedInput.isPressed(MappedInputManager::Button::Confirm)) {
    lastButtonPressTime = now;
    onSelectPressed();
    return;
  }
  if (mappedInput.isPressed(MappedInputManager::Button::Back)) {
    lastButtonPressTime = now;
    onBackPressed();
    return;
  }
  if (mappedInput.isPressed(MappedInputManager::Button::Up)) {
    lastButtonPressTime = now;
    onScrollUp();
    return;
  }
  if (mappedInput.isPressed(MappedInputManager::Button::Down)) {
    lastButtonPressTime = now;
    onScrollDown();
    return;
  }
}

void InteractiveFictionActivity::updateScrollBounds() {
  maxVisibleLines = GetVisibleLineCount(renderer.getScreenHeight());

  // Recalculate total wrapped lines for current block
  if (!textBlocks.empty()) {
    const std::string& currentText = textBlocks[currentBlockIndex];
    const int wrapWidth = GetWrapWidthChars(renderer.getScreenWidth());
    TextUtils::WrappedText wrapped = TextUtils::WrapText(currentText, wrapWidth);
    totalWrappedLines = wrapped.totalLines;
  }
}

void InteractiveFictionActivity::onSelectPressed() {
  LOG_INF("IF", "SELECT pressed");
  currentBlockIndex = (currentBlockIndex + 1) % textBlocks.size();
  scrollOffset = 0;
  updateScrollBounds();
  requestUpdate();
}

void InteractiveFictionActivity::onBackPressed() {
  LOG_INF("IF", "BACK pressed - finishing activity");
  finish();
}

void InteractiveFictionActivity::onScrollUp() {
  if (scrollOffset > 0) {
    scrollOffset--;
    LOG_INF("IF", "Scroll up - offset: %d", scrollOffset);
    requestUpdate();
  }
}

void InteractiveFictionActivity::onScrollDown() {
  int maxScroll = std::max(0, totalWrappedLines - maxVisibleLines);
  if (scrollOffset < maxScroll) {
    scrollOffset++;
    LOG_INF("IF", "Scroll down - offset: %d", scrollOffset);
    requestUpdate();
  }
}

void InteractiveFictionActivity::render(RenderLock&&) {
  // Clear the screen
  renderer.clearScreen();

  if (textBlocks.empty()) {
    renderer.displayBuffer(HalDisplay::RefreshMode::FAST_REFRESH);
    return;
  }

  // Get the current text block and wrap it using the current viewport width
  const std::string& currentText = textBlocks[currentBlockIndex];
  const int wrapWidth = GetWrapWidthChars(renderer.getScreenWidth());
  TextUtils::WrappedText wrapped = TextUtils::WrapText(currentText, wrapWidth);
  totalWrappedLines = wrapped.totalLines;

  // Get visible lines for the current viewport height
  auto visibleLines = TextUtils::GetVisibleLines(wrapped, maxVisibleLines, scrollOffset);

  // Draw title
  renderer.drawText(fontId, 10, 10, "Interactive Fiction Reader", true);

  // Draw separator
  int separatorY = 30;
  for (int x = 10; x < 1014; x += 50) {
    renderer.drawText(fontId, x, separatorY, "=", true);
  }

  // Draw text lines
  int y = kTopPaddingPx;
  for (const auto& line : visibleLines) {
    renderer.drawText(fontId, kTextInsetPx, y, line.c_str(), true);
    y += kLineHeightPx;
  }

  // Calculate pagination info
  int currentPage = (totalWrappedLines > 0) ? (scrollOffset / maxVisibleLines) + 1 : 1;
  int totalPages = (totalWrappedLines + maxVisibleLines - 1) / maxVisibleLines;

  // Draw footer with block info, page numbers, and instructions
  int footerY = 700;
  std::string blockInfo = std::string("Block ") + std::to_string(currentBlockIndex + 1) + 
                         std::string(" of ") + std::to_string(textBlocks.size());
  renderer.drawText(fontId, 10, footerY, blockInfo.c_str(), true);

  // Show page info
  std::string pageInfo = std::string("Page ") + std::to_string(currentPage) + 
                         std::string("/") + std::to_string(totalPages);
  renderer.drawText(fontId, 400, footerY, pageInfo.c_str(), true);

  // Show navigation instructions
  std::string navHints;
  if (scrollOffset > 0 || scrollOffset + maxVisibleLines < totalWrappedLines) {
    navHints = "[UP/DOWN] Scroll  [SELECT] Next  [BACK] Exit";
  } else {
    navHints = "[SELECT] Next Block   [BACK] Exit";
  }
  renderer.drawText(fontId, 10, footerY + 20, navHints.c_str(), true);

  // Request display refresh - use FAST for scroll operations
  renderer.displayBuffer(HalDisplay::RefreshMode::FAST_REFRESH);
}
