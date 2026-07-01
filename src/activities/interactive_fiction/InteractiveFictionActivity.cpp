#include "InteractiveFictionActivity.h"

#include <I18n.h>
#include <Logging.h>

#include <algorithm>

#include "TextUtils.h"
#include "YsaRuntime.h"
#include "components/UITheme.h"

namespace {
constexpr const char* kStoryPath = "/interactive_fiction/main.ysa";
constexpr int kContentPadding = 8;
constexpr int kLineHeight = 20;
constexpr int kHeaderSpacing = 6;
}  // namespace

InteractiveFictionActivity::~InteractiveFictionActivity() = default;

void InteractiveFictionActivity::onEnter() {
  LOG_INF("IF", "InteractiveFictionActivity::onEnter");

  runtime = std::make_unique<YsaRuntime>();
  loadFailed = false;
  scrollOffset = 0;

  if (!runtime->loadFromStorage(kStoryPath) || !runtime->start("Start")) {
    loadFailed = true;
    LOG_ERR("IF", "YSA load/start failed: %s", runtime->getError().c_str());
  }

  updateWrappedLines();
  scrollToBottom();
  requestUpdate();
}

void InteractiveFictionActivity::onExit() {
  LOG_INF("IF", "InteractiveFictionActivity::onExit");
  runtime.reset();
}

void InteractiveFictionActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    onSelectPressed();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onBackPressed();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    onScrollUp();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    onScrollDown();
    return;
  }
}

void InteractiveFictionActivity::onSelectPressed() {
  if (!runtime) {
    return;
  }

  if (loadFailed) {
    finish();
    return;
  }

  if (runtime->isWaitingForChoice()) {
    if (runtime->chooseSelectedOption()) {
      updateWrappedLines();
      scrollToBottom();
      requestUpdate();
    }
    return;
  }

  if (runtime->isFinished()) {
    if (runtime->start("Start")) {
      updateWrappedLines();
      scrollToBottom();
      requestUpdate();
    }
  }
}

void InteractiveFictionActivity::onBackPressed() { finish(); }

void InteractiveFictionActivity::onScrollUp() {
  if (!runtime || loadFailed) {
    return;
  }

  if (runtime->isWaitingForChoice()) {
    if (runtime->moveSelection(-1)) {
      updateWrappedLines();
      requestUpdate();
    }
    return;
  }

  if (scrollOffset > 0) {
    scrollOffset--;
    requestUpdate();
  }
}

void InteractiveFictionActivity::onScrollDown() {
  if (!runtime || loadFailed) {
    return;
  }

  if (runtime->isWaitingForChoice()) {
    if (runtime->moveSelection(1)) {
      updateWrappedLines();
      requestUpdate();
    }
    return;
  }

  const int maxScroll = std::max(0, totalWrappedLines - maxVisibleLines);
  if (scrollOffset < maxScroll) {
    scrollOffset++;
    requestUpdate();
  }
}

int InteractiveFictionActivity::getVisibleLineCount() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int contentTop = metrics.headerHeight + metrics.topPadding + kHeaderSpacing;
  const int contentBottom = renderer.getScreenHeight() - (metrics.buttonHintsHeight + kContentPadding);
  const int contentHeight = std::max(kLineHeight, contentBottom - contentTop);
  return std::max(1, contentHeight / kLineHeight);
}

void InteractiveFictionActivity::updateWrappedLines() {
  visibleLines.clear();
  maxVisibleLines = getVisibleLineCount();

  const int charsPerLine =
      std::clamp((renderer.getScreenWidth() - (kContentPadding * 2)) / 10, 18, 42);

  if (loadFailed || !runtime) {
    auto wrapped = TextUtils::WrapText(tr(STR_NO_FILES_FOUND), charsPerLine);
    visibleLines.insert(visibleLines.end(), wrapped.lines.begin(), wrapped.lines.end());
    if (runtime && runtime->hasError()) {
      auto errWrapped = TextUtils::WrapText(runtime->getError(), charsPerLine);
      visibleLines.insert(visibleLines.end(), errWrapped.lines.begin(), errWrapped.lines.end());
    }
    totalWrappedLines = static_cast<int>(visibleLines.size());
    return;
  }

  for (const auto& line : runtime->getTranscript()) {
    auto wrapped = TextUtils::WrapText(line, charsPerLine);
    visibleLines.insert(visibleLines.end(), wrapped.lines.begin(), wrapped.lines.end());
  }

  if (runtime->isWaitingForChoice()) {
    visibleLines.push_back("");
    for (const auto& option : runtime->getVisibleOptions()) {
      std::string optionLine = option.selected ? "> " : "  ";
      optionLine += option.text;
      auto wrapped = TextUtils::WrapText(optionLine, charsPerLine);
      visibleLines.insert(visibleLines.end(), wrapped.lines.begin(), wrapped.lines.end());
    }
  } else if (runtime->isFinished()) {
    visibleLines.push_back("");
    auto wrapped = TextUtils::WrapText(tr(STR_RETRY), charsPerLine);
    visibleLines.insert(visibleLines.end(), wrapped.lines.begin(), wrapped.lines.end());
  }

  totalWrappedLines = static_cast<int>(visibleLines.size());
}

void InteractiveFictionActivity::scrollToBottom() {
  const int maxScroll = std::max(0, totalWrappedLines - maxVisibleLines);
  scrollOffset = maxScroll;
}

void InteractiveFictionActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int contentTop = metrics.headerHeight + metrics.topPadding + kHeaderSpacing;

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Interactive Fiction");

  updateWrappedLines();
  auto wrapped = TextUtils::WrappedText{visibleLines, static_cast<int>(visibleLines.size())};
  const auto pageLines = TextUtils::GetVisibleLines(wrapped, maxVisibleLines, scrollOffset);

  int y = contentTop;
  for (const auto& line : pageLines) {
    renderer.drawText(kFontId, kContentPadding, y, line.c_str(), true);
    y += kLineHeight;
  }

  const char* confirmLabel = "";
  if (runtime && runtime->isWaitingForChoice()) {
    confirmLabel = tr(STR_SELECT);
  } else if (runtime && runtime->isFinished()) {
    confirmLabel = tr(STR_RETRY);
  }

  const bool canScroll = totalWrappedLines > maxVisibleLines;
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), confirmLabel, canScroll ? tr(STR_DIR_UP) : "", canScroll ? tr(STR_DIR_DOWN) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
