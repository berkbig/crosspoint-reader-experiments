#include "InteractiveFictionActivity.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>

#include "TextUtils.h"
#include "YsaRuntime.h"
#include "components/UITheme.h"

namespace {
constexpr const char* kStoryDir = "/interactive_fiction";
constexpr int kContentPadding = 8;
constexpr int kHeaderSpacing = 6;
constexpr int kLineSpacing = 2;
constexpr int kWrapMaxLinesPerInputLine = 64;
}  // namespace

InteractiveFictionActivity::~InteractiveFictionActivity() = default;

void InteractiveFictionActivity::onEnter() {
  LOG_INF("IF", "InteractiveFictionActivity::onEnter");

  runtime = std::make_unique<YsaRuntime>();
  storyFiles.clear();
  activeStoryPath.clear();
  selectedStoryIndex = 0;
  selectingStory = false;
  loadFailed = false;
  scrollOffset = 0;

  discoverStories();

  if (storyFiles.empty()) {
    loadFailed = true;
  } else if (storyFiles.size() == 1) {
    if (!loadSelectedStory()) {
      loadFailed = true;
      LOG_ERR("IF", "YSA load/start failed: %s", runtime->getError().c_str());
    }
  } else {
    selectingStory = true;
  }

  updateWrappedLines();
  scrollToBottom();
  requestUpdate();
}

void InteractiveFictionActivity::onExit() {
  LOG_INF("IF", "InteractiveFictionActivity::onExit");
  runtime.reset();
}

void InteractiveFictionActivity::discoverStories() {
  storyFiles.clear();
  const auto entries = Storage.listFiles(kStoryDir, 200);
  for (const auto& entry : entries) {
    const std::string filename = entry.c_str();
    if (FsHelpers::checkFileExtension(filename, ".ysa")) {
      storyFiles.push_back(filename);
    }
  }
  FsHelpers::sortFileList(storyFiles);
}

bool InteractiveFictionActivity::loadSelectedStory() {
  if (!runtime || selectedStoryIndex >= storyFiles.size()) {
    return false;
  }
  activeStoryPath = std::string(kStoryDir) + "/" + storyFiles[selectedStoryIndex];
  return runtime->loadFromStorage(activeStoryPath.c_str()) && runtime->start("Start");
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

  if (selectingStory) {
    if (storyFiles.empty()) {
      finish();
      return;
    }
    loadFailed = !loadSelectedStory();
    if (loadFailed) {
      LOG_ERR("IF", "YSA load/start failed: %s", runtime->getError().c_str());
      requestUpdate();
      return;
    }
    selectingStory = false;
    updateWrappedLines();
    scrollToBottom();
    requestUpdate();
    return;
  }

  if (loadFailed) {
    finish();
    return;
  }

  if (runtime->isWaitingForChoice()) {
    if (runtime->chooseSelectedOption()) {
      updateWrappedLines();
      scrollOffset = 0;
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
  if (!runtime) {
    return;
  }

  if (selectingStory) {
    if (storyFiles.empty()) {
      return;
    }
    if (selectedStoryIndex == 0) {
      selectedStoryIndex = storyFiles.size() - 1;
    } else {
      selectedStoryIndex--;
    }
    requestUpdate();
    return;
  }

  if (loadFailed) {
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
  if (!runtime) {
    return;
  }

  if (selectingStory) {
    if (storyFiles.empty()) {
      return;
    }
    selectedStoryIndex = (selectedStoryIndex + 1) % storyFiles.size();
    requestUpdate();
    return;
  }

  if (loadFailed) {
    return;
  }

  if (runtime->isWaitingForChoice()) {
    updateWrappedLines();
    const int maxScroll = std::max(0, totalWrappedLines - maxVisibleLines);
    if (scrollOffset < maxScroll) {
      scrollOffset++;
      requestUpdate();
      return;
    }
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
  const int lineHeight = getLineHeightPx();
  const int contentHeight = std::max(lineHeight, contentBottom - contentTop);
  return std::max(1, contentHeight / lineHeight);
}

int InteractiveFictionActivity::getLineHeightPx() const {
  return std::max(1, renderer.getLineHeight(kFontId) + kLineSpacing);
}

int InteractiveFictionActivity::getContentLeftPx() const {
  int viewTop = 0;
  int viewRight = 0;
  int viewBottom = 0;
  int viewLeft = 0;
  renderer.getOrientedViewableTRBL(&viewTop, &viewRight, &viewBottom, &viewLeft);
  (void)viewTop;
  (void)viewBottom;
  return std::max(kContentPadding, viewLeft + kContentPadding);
}

int InteractiveFictionActivity::getContentWidthPx() const {
  int viewTop = 0;
  int viewRight = 0;
  int viewBottom = 0;
  int viewLeft = 0;
  renderer.getOrientedViewableTRBL(&viewTop, &viewRight, &viewBottom, &viewLeft);
  (void)viewTop;
  (void)viewBottom;
  const int left = getContentLeftPx();
  const int right = std::min(renderer.getScreenWidth() - kContentPadding, renderer.getScreenWidth() - viewRight - kContentPadding);
  return std::max(20, right - left);
}

void InteractiveFictionActivity::updateWrappedLines() {
  visibleLines.clear();
  maxVisibleLines = getVisibleLineCount();
  const int contentWidth = getContentWidthPx();

  if (selectingStory) {
    for (size_t i = 0; i < storyFiles.size(); ++i) {
      std::string optionLine = (i == selectedStoryIndex) ? "> " : "  ";
      optionLine += storyFiles[i];
      const auto wrapped = renderer.wrappedText(kFontId, optionLine.c_str(), contentWidth, kWrapMaxLinesPerInputLine);
      visibleLines.insert(visibleLines.end(), wrapped.begin(), wrapped.end());
    }
    totalWrappedLines = static_cast<int>(visibleLines.size());
    if (totalWrappedLines > maxVisibleLines) {
      const int selectedLine = static_cast<int>(selectedStoryIndex);
      if (selectedLine < scrollOffset) {
        scrollOffset = selectedLine;
      } else if (selectedLine >= scrollOffset + maxVisibleLines) {
        scrollOffset = selectedLine - maxVisibleLines + 1;
      }
      const int maxScroll = std::max(0, totalWrappedLines - maxVisibleLines);
      scrollOffset = std::clamp(scrollOffset, 0, maxScroll);
    } else {
      scrollOffset = 0;
    }
    return;
  }

  if (loadFailed || !runtime) {
    auto wrapped = renderer.wrappedText(kFontId, tr(STR_NO_FILES_FOUND), contentWidth, kWrapMaxLinesPerInputLine);
    visibleLines.insert(visibleLines.end(), wrapped.begin(), wrapped.end());
    auto pathWrapped = renderer.wrappedText(kFontId, kStoryDir, contentWidth, kWrapMaxLinesPerInputLine);
    visibleLines.insert(visibleLines.end(), pathWrapped.begin(), pathWrapped.end());
    if (runtime && runtime->hasError()) {
      auto errWrapped = renderer.wrappedText(kFontId, runtime->getError().c_str(), contentWidth, kWrapMaxLinesPerInputLine);
      visibleLines.insert(visibleLines.end(), errWrapped.begin(), errWrapped.end());
    }
    totalWrappedLines = static_cast<int>(visibleLines.size());
    return;
  }

  for (const auto& line : runtime->getTranscript()) {
    auto wrapped = renderer.wrappedText(kFontId, line.c_str(), contentWidth, kWrapMaxLinesPerInputLine);
    visibleLines.insert(visibleLines.end(), wrapped.begin(), wrapped.end());
  }

  if (runtime->isWaitingForChoice()) {
    visibleLines.push_back("");
    for (const auto& option : runtime->getVisibleOptions()) {
      std::string optionLine = option.selected ? "> " : "  ";
      optionLine += option.text;
      auto wrapped = renderer.wrappedText(kFontId, optionLine.c_str(), contentWidth, kWrapMaxLinesPerInputLine);
      visibleLines.insert(visibleLines.end(), wrapped.begin(), wrapped.end());
    }
  } else if (runtime->isFinished()) {
    visibleLines.push_back("");
    auto wrapped = renderer.wrappedText(kFontId, tr(STR_RETRY), contentWidth, kWrapMaxLinesPerInputLine);
    visibleLines.insert(visibleLines.end(), wrapped.begin(), wrapped.end());
  }

  totalWrappedLines = static_cast<int>(visibleLines.size());
  const int maxScroll = std::max(0, totalWrappedLines - maxVisibleLines);
  if (runtime->isFinished()) {
    scrollOffset = maxScroll;
  } else {
    scrollOffset = std::clamp(scrollOffset, 0, maxScroll);
  }
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
  const int contentLeft = getContentLeftPx();
  const int lineHeight = getLineHeightPx();

  const char* headerTitle = "Interactive Fiction";
  if (!selectingStory && !activeStoryPath.empty()) {
    const size_t pos = activeStoryPath.find_last_of('/');
    if (pos != std::string::npos && pos + 1 < activeStoryPath.size()) {
      headerTitle = activeStoryPath.c_str() + pos + 1;
    }
  }
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, headerTitle);

  updateWrappedLines();
  auto wrapped = TextUtils::WrappedText{visibleLines, static_cast<int>(visibleLines.size())};
  const auto pageLines = TextUtils::GetVisibleLines(wrapped, maxVisibleLines, scrollOffset);

  int y = contentTop;
  for (const auto& line : pageLines) {
    renderer.drawText(kFontId, contentLeft, y, line.c_str(), true);
    y += lineHeight;
  }

  const char* confirmLabel = "";
  if (selectingStory) {
    confirmLabel = tr(STR_SELECT);
  } else if (runtime && runtime->isWaitingForChoice()) {
    confirmLabel = tr(STR_SELECT);
  } else if (runtime && runtime->isFinished()) {
    confirmLabel = tr(STR_RETRY);
  }

  const bool canScroll = selectingStory || totalWrappedLines > maxVisibleLines;
  const bool showScrollPrompt = runtime && runtime->isWaitingForChoice() && scrollOffset < std::max(0, totalWrappedLines - maxVisibleLines);
  if (showScrollPrompt) {
    const int promptY = renderer.getScreenHeight() - metrics.buttonHintsHeight - renderer.getLineHeight(kFontId) - 2;
    renderer.drawCenteredText(kFontId, promptY, tr(STR_SCROLL), true, EpdFontFamily::BOLD);
  }
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), confirmLabel, canScroll ? tr(STR_DIR_UP) : "", canScroll ? tr(STR_DIR_DOWN) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
