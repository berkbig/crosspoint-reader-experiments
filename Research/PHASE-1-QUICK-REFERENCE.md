# Phase 1: Barebones Activity - Quick Reference Card

## TL;DR
Build a simple Crosspoint activity that displays hardcoded text and cycles through different text blocks on button press. Takes ~1 week.

## Final Deliverable
- Compiling activity that shows Lorem Ipsum text
- SELECT button cycles to next text block
- BACK button exits to home
- Multi-line text wraps and renders correctly
- No YarnSpinner, no branching, no files

## 6 Tasks (6-7 Focused Commits)

### Task 1.1: Directory Structure (1 hour)
```
src/activities/interactive_fiction/
├── InteractiveFictionActivity.h
├── InteractiveFictionActivity.cpp
├── TextUtils.h
└── TextUtils.cpp
```
- Create files
- Add to build system
- Verify empty project compiles

**Commit:** `refactor: Create IF activity directory structure`

---

### Task 1.2: Activity Skeleton (2 hours)
Create `InteractiveFictionActivity` extending `Activity`:
- Members: `std::vector<std::string> textBlocks`, `currentBlockIndex`, `scrollOffset`
- Methods: `onEnter()`, `onExit()`, `loop()`, `render()`
- Test data: 4 Lorem Ipsum strings

**Key Code:**
```cpp
class InteractiveFictionActivity : public Activity {
    std::vector<std::string> textBlocks;
    int currentBlockIndex = 0;
    int scrollOffset = 0;
    
    void generateTestBlocks();
    void onSelectPressed();
    void onBackPressed();
};
```

**Commit:** `feat: Add InteractiveFictionActivity skeleton`

---

### Task 1.3: Text Wrapping Utility (2 hours)
Create `TextUtils` with word wrapping:
- `WrapText(text, maxCharsPerLine)` → returns wrapped lines
- `GetVisibleLines(wrapped, maxVisible, scrollOffset)` → returns subset for display
- Simple algorithm: break on word boundaries

**Key Code:**
```cpp
TextUtils::WrappedText WrapText(const std::string& text, int maxCharsPerLine);
std::vector<std::string> GetVisibleLines(const WrappedText& wrapped, int maxVisibleLines, int scrollOffset);
```

**Commit:** `feat: Add TextUtils for text wrapping`

---

### Task 1.4: Rendering (2 hours)
Implement `render()` method:
- Clear canvas
- Draw title ("Interactive Fiction Reader")
- Draw wrapped text lines
- Draw footer (block number, instructions)
- Request display refresh

**Key Code:**
```cpp
void InteractiveFictionActivity::render(RenderLock&&) {
    auto& canvas = renderer.getCanvas();
    canvas.clear();
    canvas.drawText(10, 10, "Interactive Fiction Reader");
    
    auto visibleLines = TextUtils::GetVisibleLines(wrappedText, 15, scrollOffset);
    int y = 50;
    for (const auto& line : visibleLines) {
        canvas.drawText(10, y, line);
        y += 20;
    }
    
    canvas.drawText(10, 740, "[SELECT] Next   [BACK] Exit");
    renderer.requestRefresh(GfxRenderer::RefreshMode::FULL);
}
```

**Commit:** `feat: Implement text rendering`

---

### Task 1.5: Input Handling (1.5 hours)
Implement `loop()` and button handlers:
- Check `mappedInput.isPressed(Button::SELECT)` → cycle text blocks
- Check `mappedInput.isPressed(Button::BACK)` → exit activity
- Update state, call `requestUpdate()`

**Key Code:**
```cpp
void InteractiveFictionActivity::loop() {
    if (mappedInput.isPressed(MappedInputManager::Button::SELECT)) {
        onSelectPressed();
    }
    if (mappedInput.isPressed(MappedInputManager::Button::BACK)) {
        onBackPressed();
    }
}

void InteractiveFictionActivity::onSelectPressed() {
    currentBlockIndex = (currentBlockIndex + 1) % textBlocks.size();
    scrollOffset = 0;
    requestUpdate();
}

void InteractiveFictionActivity::onBackPressed() {
    finish();
}
```

**Commit:** `feat: Add button input handling`

---

### Task 1.6: Hardware Integration Test (1.5 hours)
Manual testing on Crosspoint device or simulator:
- [ ] Activity launches from home screen
- [ ] Text displays and is readable
- [ ] SELECT button advances to next block
- [ ] SELECT cycles through all 4 blocks
- [ ] Text wrapping works correctly
- [ ] BACK returns to home
- [ ] No crashes or hangs
- [ ] Display refresh ~500-600ms (acceptable)
- [ ] Button press responsiveness <100ms

**Commit:** `test: Verify Phase 1 integration on hardware`

---

## Estimated Schedule

| Task | Hours | Day(s) | Cumulative |
|------|-------|--------|-----------|
| 1.1: Directory | 1 | 0.5 | 1h |
| 1.2: Skeleton | 2 | 1 | 3h |
| 1.3: TextUtils | 2 | 1 | 5h |
| 1.4: Rendering | 2 | 1 | 7h |
| 1.5: Input | 1.5 | 0.5 | 8.5h |
| 1.6: Test | 1.5 | 1 | 10h |
| **TOTAL** | **10** | **4.5** | **10h** |

*Estimate: 4-5 working days for one developer*

## Coding Standards

✅ **DO:**
- Use `const` and references where appropriate
- Follow Crosspoint naming conventions (CamelCase for classes)
- Log via `Logging.h` / `LOG_I()`
- Use HAL abstractions (no direct SDK calls)
- Add comments for non-obvious logic
- One concern per commit

❌ **DON'T:**
- Don't add YarnSpinner dependencies yet
- Don't implement file browser
- Don't add save/load
- Don't optimize before it works
- Don't skip testing on hardware

## Definition of Done

Phase 1 is complete when:

- ✅ All 6 commits are made with clear messages
- ✅ Code compiles without warnings
- ✅ Activity instantiates and launches
- ✅ Text displays and is readable
- ✅ Button input works responsively
- ✅ Exits cleanly to home
- ✅ Tested on actual hardware (or high-fidelity simulator)
- ✅ Code review approves integration pattern
- ✅ No known bugs or crashes

## Success Metrics

After Phase 1, you will have:

**Functional:**
- A working Activity in Crosspoint
- Proof that Activity integration works
- Multi-line text rendering that works
- Button input fully functional

**Technical:**
- Clean, reviewable code
- Clear integration pattern for future phases
- Foundation for Phase 3 (YarnSpinner)

**Team:**
- Confidence in the approach
- Working prototype to show stakeholders
- Early feedback on display/input experience

## Next Steps After Phase 1

Once Phase 1 is approved and merged:

1. **Phase 2** (Week 1.5-2): Enhance text rendering
   - Word-break wrapping
   - Vertical scrolling with UP/DOWN
   - Pagination indicators

2. **Phase 3** (Week 1.5-2.5): Port YarnSpinner Core
   - In parallel with Phase 2
   - No Activity integration yet
   - Just get VM working in isolation

3. **Phase 4** (Week 2.5-3): Wire it all together
   - Replace dummy text with VM output
   - Handle dialogue, options, branching

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Activity doesn't launch | Check ActivityManager registration, import |
| Text doesn't appear | Verify canvas drawing, check font/size |
| Button not responsive | Check MappedInputManager config, button names |
| Display refresh too slow | Expected ~500-600ms; if >1s, check HAL |
| Memory issues | Profile heap usage; text blocks should be <50KB |
| Compilation errors | Check Logging.h includes, Activity base class |

## File Checklist

Create these files:
- [ ] `src/activities/interactive_fiction/InteractiveFictionActivity.h`
- [ ] `src/activities/interactive_fiction/InteractiveFictionActivity.cpp`
- [ ] `src/activities/interactive_fiction/TextUtils.h`
- [ ] `src/activities/interactive_fiction/TextUtils.cpp`

Modify these files:
- [ ] `platformio.ini` (add include path if needed)
- [ ] Activity registration in home/activity manager

## Testing Checklist

Before declaring Phase 1 done:

- [ ] Compiles without errors or warnings
- [ ] No memory leaks (basic heap check)
- [ ] Activity launches from home
- [ ] Text displays (readable, not cut off)
- [ ] SELECT cycles text
- [ ] BACK exits
- [ ] No crashes on rapid button pressing
- [ ] Display refresh feels smooth
- [ ] Button responsiveness acceptable (<100ms)
- [ ] Code review passes

---

**Start Date:** [TBD]
**Target Completion:** [TBD + 5 working days]
**Reviewer:** [TBD]
