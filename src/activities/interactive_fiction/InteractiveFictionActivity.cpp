#include "InteractiveFictionActivity.h"
#include "TextUtils.h"
#include "Logging.h"
#include "HalDisplay.h"

void InteractiveFictionActivity::generateTestBlocks() {
  textBlocks = {
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
      "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
      "Ut enim ad minim veniam, quis nostrud exercitation.",

      "Ullamco laboris nisi ut aliquip ex ea commodo consequat. "
      "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum. "
      "Dolore eu fugiat nulla pariatur excepteur sint occaecat.",

      "Cupidatat non proident, sunt in culpa qui officia deserunt mollit anim. "
      "Id est laborum sed ut perspiciatis unde omnis iste natus error. "
      "Sit voluptatem accusantium doloremque laudantium.",

      "Totam rem aperiam, eaque ipsa quae ab illo inventore veritatis. "
      "Et quasi architecto beatae vitae dicta sunt explicabo. "
      "Nemo enim ipsam voluptatem quia voluptas sit aspernatur."
  };
}

void InteractiveFictionActivity::onEnter() {
  LOG_INF("IF", "InteractiveFictionActivity::onEnter");
  generateTestBlocks();
  currentBlockIndex = 0;
  scrollOffset = 0;
  requestUpdate();
}

void InteractiveFictionActivity::onExit() {
  LOG_INF("IF", "InteractiveFictionActivity::onExit");
}

void InteractiveFictionActivity::loop() {
  if (mappedInput.isPressed(MappedInputManager::Button::Confirm)) {
    onSelectPressed();
  }
  if (mappedInput.isPressed(MappedInputManager::Button::Back)) {
    onBackPressed();
  }
}

void InteractiveFictionActivity::onSelectPressed() {
  LOG_INF("IF", "SELECT pressed");
  currentBlockIndex = (currentBlockIndex + 1) % textBlocks.size();
  scrollOffset = 0;
  requestUpdate();
}

void InteractiveFictionActivity::onBackPressed() {
  LOG_INF("IF", "BACK pressed - finishing activity");
  finish();
}

void InteractiveFictionActivity::render(RenderLock&&) {
  // Clear the screen
  renderer.clearScreen();

  if (textBlocks.empty()) {
    renderer.displayBuffer(HalDisplay::RefreshMode::FAST_REFRESH);
    return;
  }

  // Get the current text block and wrap it
  const std::string& currentText = textBlocks[currentBlockIndex];
  TextUtils::WrappedText wrapped = TextUtils::WrapText(currentText, 80);

  // Get visible lines (approximately 15 lines fit on screen)
  auto visibleLines = TextUtils::GetVisibleLines(wrapped, 15, scrollOffset);

  // Draw title
  renderer.drawText(fontId, 10, 10, "Interactive Fiction Reader", true);

  // Draw separator
  int separatorY = 30;
  for (int x = 10; x < 1014; x += 50) {
    renderer.drawText(fontId, x, separatorY, "=", true);
  }

  // Draw text lines
  int y = 50;
  int lineHeight = 20;
  for (const auto& line : visibleLines) {
    renderer.drawText(fontId, 10, y, line.c_str(), true);
    y += lineHeight;
  }

  // Draw footer with block info and instructions
  int footerY = 720;
  std::string blockInfo = std::string("Block ") + std::to_string(currentBlockIndex + 1) + 
                         std::string(" of ") + std::to_string(textBlocks.size());
  renderer.drawText(fontId, 10, footerY, blockInfo.c_str(), true);
  renderer.drawText(fontId, 10, footerY + 20, "[SELECT] Next   [BACK] Exit", true);

  // Request display refresh
  renderer.displayBuffer(HalDisplay::RefreshMode::FAST_REFRESH);
}
