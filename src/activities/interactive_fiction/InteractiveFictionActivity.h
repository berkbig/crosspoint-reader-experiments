#pragma once

#include "activities/Activity.h"
#include "fontIds.h"
#include <string>
#include <vector>
#include <memory>

class TextUtils;

class InteractiveFictionActivity final : public Activity {
  std::vector<std::string> textBlocks;
  int currentBlockIndex = 0;
  int scrollOffset = 0;
  const int fontId = UI_10_FONT_ID;

  // Helper methods
  void generateTestBlocks();
  void onSelectPressed();
  void onBackPressed();

 public:
  explicit InteractiveFictionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("InteractiveFiction", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
