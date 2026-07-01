#pragma once

#include "activities/Activity.h"
#include "fontIds.h"
#include "YsaRuntime.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class InteractiveFictionActivity final : public Activity {
  std::unique_ptr<YsaRuntime> runtime;

  std::vector<std::string> visibleLines;
  std::vector<std::string> storyFiles;
  std::string activeStoryPath;
  size_t selectedStoryIndex = 0;
  int scrollOffset = 0;
  int maxVisibleLines = 13;
  int totalWrappedLines = 0;
  static constexpr int kFontId = UI_12_FONT_ID;
  bool loadFailed = false;
  bool selectingStory = false;

  // Helper methods
  void discoverStories();
  bool loadSelectedStory();
  void onSelectPressed();
  void onBackPressed();
  void onScrollUp();
  void onScrollDown();
  void updateWrappedLines();
  void scrollToBottom();
  int getVisibleLineCount() const;

 public:
  explicit InteractiveFictionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("InteractiveFiction", renderer, mappedInput) {}
  ~InteractiveFictionActivity() override;

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
