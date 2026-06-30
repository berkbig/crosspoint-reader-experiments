#pragma once

#include "activities/Activity.h"
#include "fontIds.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

class TextUtils;

class InteractiveFictionActivity final : public Activity {
  std::vector<std::string> textBlocks;
  int currentBlockIndex = 0;
  int scrollOffset = 0;
  int maxVisibleLines = 13;
  int totalWrappedLines = 0;
  const int fontId = UI_12_FONT_ID;
  uint32_t lastButtonPressTime = 0;
  static constexpr uint32_t DEBOUNCE_MS = 200;

  // Helper methods
  void generateTestBlocks();
  void onSelectPressed();
  void onBackPressed();
  void onScrollUp();
  void onScrollDown();
  void updateScrollBounds();

 public:
  explicit InteractiveFictionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("InteractiveFiction", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
