# Interactive Fiction Runtime - Milestone Plan

## Project Phases Overview

The implementation is structured in 7 phases across approximately 5 weeks of development work. Each phase delivers a functional, testable component.

**Key Philosophy:** Get integration working first (barebones activity), then layer on YarnSpinner complexity.

```
Phase 1: Barebones Activity Skeleton
├─ Weeks 1 (~8-12 hours)
├─ Deliverable: IF Activity that displays text & responds to input
└─ Risk: None; proves Activity integration works

Phase 2: Multi-Line Text & Pagination
├─ Weeks 1-1.5 (~6-8 hours)
├─ Deliverable: Text wrapping, page navigation
└─ Risk: Display refresh optimization

Phase 3: YarnSpinner Core Adaptation (Host Validation)
├─ Weeks 1.5-2.5 (~20 hours)
├─ Deliverable: YarnSpinner behavior validated in host harness
└─ Risk: Runtime/schema drift across YarnSpinner versions

Phase 4: YarnC -> Device-Native Asset Compiler
├─ Weeks 2.5-3 (~12 hours)
├─ Deliverable: Offline compiler that emits MCU-optimized story assets
└─ Risk: Converter correctness and format versioning

Phase 5: Firmware Runtime Integration
├─ Weeks 3-3.5 (~16 hours)
├─ Deliverable: Activity executes native assets without protobuf runtime
└─ Risk: State management complexity

Phase 6: Session Management
├─ Weeks 3.5-4.5 (~16 hours)
├─ Deliverable: File browser, save/load, bookmarks
└─ Risk: Storage layer edge cases

Phase 7: Polish & Optimization
├─ Weeks 4.5-5 (~16 hours)
├─ Deliverable: Complete, tested, optimized
└─ Risk: Performance tuning

Total estimated effort: 94-100 hours
```

---

## Phase 1: Barebones Activity Skeleton (Week 1, ~8-12 hours)

### Goals
- [ ] Create a minimal InteractiveFictionActivity that compiles and runs
- [ ] Display a hardcoded multi-line text block
- [ ] Respond to button presses to cycle through random text blocks
- [ ] Prove integration with Crosspoint Activity system works
- [ ] **NO YarnSpinner dependencies yet**

### Why This First?
This milestone is a **integration test** that proves:
1. The Activity lifecycle works (onEnter, loop, render, onExit)
2. The rendering system can display multi-line text
3. Button input is correctly routed
4. The activity can be registered and launched

Debugging these issues first makes YarnSpinner integration (Phase 3) much easier.

### Deliverables

#### 1.1 Project Setup & Directory Structure
**Effort:** 1 hour
**Owner:** Senior dev

**Tasks:**
1. Create directory structure:
   ```
   src/activities/interactive_fiction/
   ├── InteractiveFictionActivity.h
   ├── InteractiveFictionActivity.cpp
   ├── TextUtils.h          (text wrapping helper)
   └── TextUtils.cpp
   ```

2. Add includes to build system
3. Verify build succeeds with empty activity

**Checklist:**
- [ ] Directory created
- [ ] Files created (empty implementations)
- [ ] Project compiles

**PR Title:** `feat: Create interactive fiction activity directory structure`

---

#### 1.2 Activity Class Skeleton
**Effort:** 2 hours
**Owner:** Dev 1

**Implementation:**

Create `InteractiveFictionActivity.h`:
```cpp
#pragma once

#include "activities/Activity.h"
#include <string>
#include <vector>

class InteractiveFictionActivity final : public Activity {
    // Test data
    std::vector<std::string> textBlocks;
    int currentBlockIndex = 0;
    
    // UI state
    int scrollOffset = 0;  // For future pagination
    
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
```

Create `InteractiveFictionActivity.cpp`:
```cpp
#include "InteractiveFictionActivity.h"
#include "Logging.h"

void InteractiveFictionActivity::generateTestBlocks() {
    textBlocks = {
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit.\n"
        "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.",
        
        "Ut enim ad minim veniam, quis nostrud exercitation ullamco.\n"
        "Laboris nisi ut aliquip ex ea commodo consequat duis aute.",
        
        "Duis aute irure dolor in reprehenderit in voluptate velit.\n"
        "Esse cillum dolore eu fugiat nulla pariatur excepteur sint.",
        
        "Excepteur sint occaecat cupidatat non proident, sunt in culpa.\n"
        "Qui officia deserunt mollit anim id est laborum sed ut."
    };
}

void InteractiveFictionActivity::onEnter() {
    LOG_I("IF", "InteractiveFictionActivity::onEnter");
    generateTestBlocks();
    currentBlockIndex = 0;
    scrollOffset = 0;
    requestUpdate();
}

void InteractiveFictionActivity::onExit() {
    LOG_I("IF", "InteractiveFictionActivity::onExit");
}

void InteractiveFictionActivity::loop() {
    if (mappedInput.isPressed(MappedInputManager::Button::SELECT)) {
        onSelectPressed();
    }
    if (mappedInput.isPressed(MappedInputManager::Button::BACK)) {
        onBackPressed();
    }
}

void InteractiveFictionActivity::onSelectPressed() {
    LOG_I("IF", "SELECT pressed");
    currentBlockIndex = (currentBlockIndex + 1) % textBlocks.size();
    scrollOffset = 0;
    requestUpdate();
}

void InteractiveFictionActivity::onBackPressed() {
    LOG_I("IF", "BACK pressed - finishing activity");
    finish();
}

void InteractiveFictionActivity::render(RenderLock&&) {
    // Placeholder: just clear and request refresh
    renderer.requestRefresh(GfxRenderer::RefreshMode::FULL);
}
```

**Checklist:**
- [ ] Header compiles
- [ ] Cpp compiles
- [ ] Activity instantiates without errors
- [ ] onEnter/onExit/loop methods callable

**PR Title:** `feat: Add InteractiveFictionActivity skeleton with test blocks`

---

#### 1.3 Text Rendering Helper
**Effort:** 2 hours
**Owner:** Dev 2

**Implementation:**

Create `TextUtils.h`:
```cpp
#pragma once
#include <string>
#include <vector>

class TextUtils {
public:
    struct WrappedText {
        std::vector<std::string> lines;
        int totalLines;
    };
    
    // Wrap text to fit within maxWidth pixels
    // Assumes monospace font; maxWidth = chars * charWidth
    static WrappedText WrapText(const std::string& text, int maxCharsPerLine);
    
    // Get visible portion given scroll offset
    static std::vector<std::string> GetVisibleLines(
        const WrappedText& wrapped,
        int maxVisibleLines,
        int scrollOffset
    );
};
```

Create `TextUtils.cpp`:
```cpp
#include "TextUtils.h"
#include <sstream>
#include <algorithm>

TextUtils::WrappedText TextUtils::WrapText(const std::string& text, int maxCharsPerLine) {
    WrappedText result;
    std::string line;
    
    for (char c : text) {
        if (c == '\n') {
            result.lines.push_back(line);
            line.clear();
        } else if (line.length() >= maxCharsPerLine) {
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
        visible.push_back(wrapped.lines[i]);
    }
    return visible;
}
```

**Checklist:**
- [ ] TextUtils compiles
- [ ] WrapText handles basic cases
- [ ] GetVisibleLines returns correct subset

**PR Title:** `feat: Add text wrapping utility`

---

#### 1.4 Basic Rendering Implementation
**Effort:** 2 hours
**Owner:** Dev 1

**Update `InteractiveFictionActivity.cpp`:**

```cpp
#include "TextUtils.h"

// Add to class header:
private:
    TextUtils::WrappedText wrappedText;

// Update render():
void InteractiveFictionActivity::render(RenderLock&&) {
    LOG_I("IF", "Rendering block %d", currentBlockIndex);
    
    // Get current text block
    const std::string& currentText = textBlocks[currentBlockIndex];
    
    // Wrap text (assuming ~80 chars per line at default font size)
    wrappedText = TextUtils::WrapText(currentText, 80);
    
    // Get visible lines (assume ~15 lines fit on screen)
    auto visibleLines = TextUtils::GetVisibleLines(wrappedText, 15, scrollOffset);
    
    // Clear canvas
    auto& canvas = renderer.getCanvas();
    canvas.clear();
    
    // Draw title
    canvas.drawText(10, 10, "Interactive Fiction Reader");
    canvas.drawText(10, 25, "==========================");
    
    // Draw text
    int y = 50;
    for (const auto& line : visibleLines) {
        canvas.drawText(10, y, line);
        y += 20;
    }
    
    // Draw footer
    int footerY = 740;
    canvas.drawText(10, footerY, "[SELECT] Next   [BACK] Exit");
    canvas.drawText(10, footerY - 20, 
        std::string("Block ") + std::to_string(currentBlockIndex + 1) + 
        std::string(" of ") + std::to_string(textBlocks.size()));
    
    // Request display update
    renderer.requestRefresh(GfxRenderer::RefreshMode::FULL);
}
```

**Checklist:**
- [ ] Compiles without errors
- [ ] Activity can be launched in Crosspoint
- [ ] Text displays on screen (visible in simulator/hardware)
- [ ] Text doesn't get cut off (readable)

**PR Title:** `feat: Implement text rendering in IF activity`

---

#### 1.5 Input Handling & State Transitions
**Effort:** 1.5 hours
**Owner:** Dev 2

**Verify & debug:**
1. Button presses are correctly recognized
2. Block cycling works (SELECT increments)
3. BACK returns to home
4. No repeated triggers on held button

**Manual Testing Checklist:**
- [ ] Launch activity from home screen
- [ ] Text displays on screen
- [ ] Press SELECT → text changes to next block
- [ ] Press SELECT multiple times → cycles through all blocks
- [ ] Press SELECT on last block → wraps to first block
- [ ] Press BACK → returns to home screen
- [ ] No crashes or hang

**PR Title:** `test: Verify input handling and state transitions work`

---

#### 1.6 Integration Test on Hardware (or Simulator)
**Effort:** 1.5 hours
**Owner:** Senior dev

**Pre-flight Checklist:**
- [ ] Code compiles without warnings
- [ ] No obvious memory leaks (basic inspection)
- [ ] Activity is registered in ActivityManager
- [ ] Activity appears in home menu or file browser

**Hardware/Simulator Test:**
- [ ] Launch activity
- [ ] Verify text renders clearly
- [ ] Verify button responses are responsive (<100ms latency)
- [ ] No display artifacts or glitches
- [ ] Screen refresh is smooth
- [ ] Exit works cleanly

**PR Title:** `feat: Phase 1 complete - Barebones IF activity working`

---

### Phase 1 Summary

**Total Effort:** 8-12 hours (across ~3-4 days)

**What You Get:**
- ✅ A working Activity that launches
- ✅ Multi-line text display with wrapping
- ✅ Button input handling
- ✅ Proof of concept for Activity integration
- ✅ Foundation for YarnSpinner (Phase 3)

**What You Don't Get (saved for later phases):**
- ❌ YarnSpinner VM
- ❌ Dialogue branching/options
- ❌ State persistence
- ❌ File browser integration

**Commits (should be ~6-7 focused, single-concern commits):**
1. `refactor: Create IF activity directory structure`
2. `feat: Add InteractiveFictionActivity skeleton`
3. `feat: Add TextUtils for text wrapping`
4. `feat: Implement basic text rendering`
5. `feat: Add input handling and state cycling`
6. `test: Verify hardware integration`
7. `docs: Add IF activity integration notes`

**Review Criteria:**
- [ ] Activity follows Crosspoint patterns (Activity base class, lifecycle hooks)
- [ ] Uses HAL abstractions (no direct SDK calls)
- [ ] Button names use MappedInputManager logical names
- [ ] Logging via Logging.h
- [ ] No memory leaks
- [ ] Code style consistent with repo
- [ ] Compiles without warnings

---

## Phase 2: Multi-Line Text & Pagination (Weeks 1-1.5, ~6-8 hours)

### Goals
- [ ] Improve text wrapping (word-break, not character-break)
- [ ] Implement vertical pagination (UP/DOWN to scroll)
- [ ] Add visual indicators (page numbers, scroll position)
- [ ] Optimize display refresh (use PARTIAL for scrolling)

### Deliverables

#### 2.1 Smart Word-Wrapping
**Effort:** 3 hours

Enhance `TextUtils::WrapText()` to:
- Break on word boundaries (not mid-word)
- Handle hyphenated words
- Preserve intentional newlines

#### 2.2 Vertical Scroll Navigation
**Effort:** 2 hours

Add to `InteractiveFictionActivity`:
- UP button: scroll up one line
- DOWN button: scroll down one line
- Track scroll offset, clamp to valid range

#### 2.3 Visual Pagination Indicators
**Effort:** 1.5 hours

Render:
- Current page number (e.g., "Page 2/5")
- Scrollbar indicator
- "[↑] Scroll up / [↓] Scroll down" instructions

#### 2.4 Display Refresh Optimization
**Effort:** 1.5 hours

- Use FULL refresh only on block change
- Use PARTIAL refresh for scroll operations
- Profile display latency

### Phase 2 Commits
1. `feat: Implement word-break wrapping`
2. `feat: Add vertical scroll navigation`
3. `feat: Add pagination indicators`
4. `perf: Optimize display refresh strategy`

---

## Phase 3: YarnSpinner Core Adaptation (Weeks 1.5-2.5, ~20 hours)

### Goals
- [ ] Adapt YarnSpinner Core C++ code to run on Crosspoint (remove UE dependencies)
- [ ] Implement `IVariableStorage` and `ILogger` concrete classes
- [ ] Create unit tests for VM execution
- [ ] Get a simple `.yarn` file executing end-to-end

### Deliverables

#### 3.1 Protobuf Setup (Host-Side Only)
**Effort:** 4 hours

**Tasks:**
1. Pin YarnSpinner console/protobuf versions for host build reproducibility
2. Generate/load protobuf messages in harness/compiler tooling (not firmware runtime)
3. Copy protobuf-generated files (`.pb.h`, `.pb.cc`) into host tooling project
4. Create build configuration for protobuf code generation (if needed)

**Definition of Done:**
- [ ] Host protobuf library compiles without errors
- [ ] `yarn_spinner.pb.h` and `compiler_output.pb.h` can be included
- [ ] Simple protobuf message serialization/deserialization works
- [ ] Firmware target has no protobuf dependency added

**Location:** `src/lib/yarnspinner_core/`

---

#### 3.2 Porting YarnSpinner Core
**Effort:** 16 hours

**Tasks:**
1. Copy core files to `src/lib/yarnspinner_core/`:
   - `Common.h` → adapt for Crosspoint logging
   - `Value.h` → remove UE types
   - `State.h/cpp` → adapt container types
   - `Library.h/cpp` → adapt function registry
   - `VirtualMachine.h/cpp` → **largest file, requires heavy editing**

2. Search & replace Unreal types:
   ```
   - YARNSPINNER_API → (remove)
   - FString → std::string
   - TArray<T> → std::vector<T>
   - TMap<K,V> → std::map<K,V>
   - check() → assert()
   - UE_LOG → (replace with ILogger calls)
   ```

3. Fix #includes:
   - Remove `Containers/Map.h`, `Containers/Array.h`
   - Add `<map>`, `<vector>`, `<string>`
   - Ensure `Logging.h` is available for logging impl

4. Fix Unreal-specific patterns:
   - Constructor initializer lists (UE style → C++ style)
   - Shared pointer patterns (`TSharedPtr` → `std::shared_ptr`)
   - Any platform-specific code (e.g., `#ifdef WITH_EDITOR`)

**Definition of Done:**
- [ ] All files compile without Unreal dependencies
- [ ] No linker errors for YarnSpinner symbols
- [ ] VirtualMachine class is constructible and callable
- [ ] Code review passes (no hardcoded values, sensible memory management)

**Location:** `src/lib/yarnspinner_core/`

---

#### 3.3 Concrete Implementations
**Effort:** 6 hours

**Task 1: YarnVariableStorage (IVariableStorage)**

File: `src/lib/yarnspinner_core/YarnVariableStorage.h/cpp`

```cpp
class YarnVariableStorage : public Yarn::IVariableStorage {
    std::map<std::string, Yarn::Value> variables;
    
    void SetValue(std::string name, bool value) override;
    void SetValue(std::string name, float value) override;
    void SetValue(std::string name, std::string value) override;
    bool HasValue(std::string name) override;
    Yarn::Value GetValue(std::string name) override;
    void ClearValue(std::string name) override;
    
    // Crosspoint-specific extensions
    void LoadFromJSON(const std::string& json);
    std::string SaveToJSON() const;
};
```

**Task 2: YarnLogger (ILogger)**

File: `src/lib/yarnspinner_core/YarnLogger.h/cpp`

```cpp
class YarnLogger : public Yarn::ILogger {
    void Log(std::string message, Type severity) override;
};
```

Bridge to Crosspoint's `Logging.h` system.

**Definition of Done:**
- [ ] YarnVariableStorage stores/retrieves all value types
- [ ] JSON serialization round-trips correctly
- [ ] YarnLogger outputs to serial/logging system
- [ ] Unit tests pass (set/get boolean, number, string; type coercion)

---

#### 3.4 VM Adapter Layer
**Effort:** 4 hours

File: `src/lib/yarnspinner_core/YarnSpinnerVM.h/cpp`

```cpp
class YarnSpinnerVM {
    std::unique_ptr<Yarn::VirtualMachine> vm;
    std::unique_ptr<Yarn::IVariableStorage> varStorage;
    std::unique_ptr<Yarn::ILogger> logger;
    
    // Content delivery buffers
    std::optional<Yarn::Line> currentLine;
    std::optional<Yarn::OptionSet> currentOptions;
    
public:
    // Lifecycle
    bool LoadProgram(const std::string& filePath);
    void UnloadProgram();
    
    // Execution
    bool SetNode(const std::string& nodeName);
    Yarn::VirtualMachine::ExecutionState GetState() const;
    bool Continue();
    
    // Option handling
    void SelectOption(int index);
    
    // Content access
    const Yarn::Line* GetCurrentLine() const;
    const Yarn::OptionSet* GetCurrentOptions() const;
    
    // State persistence
    std::string SerializeState() const;  // JSON
    bool DeserializeState(const std::string& json);
};
```

**Definition of Done:**
- [ ] VM can be constructed with storage & logger
- [ ] Program file can be loaded from disk
- [ ] Simple node execution returns expected content

---

#### 3.5 Unit Tests
**Effort:** 4 hours

Location: `test/lib/yarnspinner_core/`

**Test Cases:**
```cpp
// Value type tests
TEST(YarnValue, BoolToString)
TEST(YarnValue, NumberToString)
TEST(YarnValue, CoerceTypes)

// Storage tests
TEST(YarnVariableStorage, SetGetBoolean)
TEST(YarnVariableStorage, SetGetNumber)
TEST(YarnVariableStorage, SetGetString)
TEST(YarnVariableStorage, JSONRoundTrip)

// VM tests
TEST(YarnVM, LoadProgram)
TEST(YarnVM, SimpleLinearExecution)
TEST(YarnVM, BranchingExecution)
TEST(YarnVM, OptionSelection)
```

**Definition of Done:**
- [ ] All tests pass locally
- [ ] Code coverage >80% for core classes
- [ ] Test results documented in pull request

---

### Phase 3 Review Checklist

Before moving to Phase 4:
- [ ] All YarnSpinner Core files compile
- [ ] No Unreal dependencies remain
- [ ] Unit tests pass
- [ ] Code follows Crosspoint conventions (spacing, naming, HAL usage)
- [ ] Memory profiling shows reasonable heap usage (<200KB for test story)
- [ ] Pull request approved by repository maintainer

---

## Phase 4: YarnC -> Device-Native Asset Compiler (Weeks 2.5-3, ~12 hours)

### Goals
- [ ] Define a compact, versioned runtime asset format optimized for ESP32
- [ ] Implement a converter from YarnSpinner `.yarnc` + line tables to native assets
- [ ] Validate converter output against harness behavior using golden tests
- [ ] Document the format and compatibility guarantees

### Deliverables
1. **Format Spec** - instruction encoding, line table layout, jump/option representation, version header
2. **Compiler Tool** - deterministic converter executable/script in `tools/`
3. **Validation Suite** - sample projects compiled both ways with expected transcript comparison
4. **Integration Contract** - firmware loader contract for native assets

### Definition of Done
- [ ] Converter emits deterministic output for the same `.yarnc` input
- [ ] Native asset output is smaller and/or lower-overhead than loading protobuf on-device
- [ ] Golden transcript tests pass for branching and options
- [ ] Format versioning and upgrade path documented

---

## Phase 5: Firmware Runtime Integration (Weeks 3-3.5, ~16 hours)

### Goals
- [ ] Integrate native-format runtime into InteractiveFictionActivity
- [ ] Replace dummy text with native asset execution
- [ ] Handle line/option callbacks from runtime
- [ ] Test with compiled native assets

### Deliverables (Brief Summary)

1. **Runtime Initialization** - Load native story asset in activity onEnter
2. **Dialogue Loop** - Wire advance/select input to runtime step functions
3. **Option Selection** - Display options, handle selection
4. **State Callbacks** - Display lines and options as they arrive

See Phase 3 and Phase 4 for conversion/runtime details.

---

## Phase 6: Session Management (Weeks 3.5-4.5, ~16 hours)

### Goals
- [ ] Integrate with Crosspoint file browser for compiled story asset selection
- [ ] Implement save/load state persistence
- [ ] Create bookmarks (save named positions in dialogue)

### Deliverables (Brief Summary)

1. **File Browser Integration** - Register native story asset extension, enable file selection
2. **State Persistence** - Save/load via JSON to SD card
3. **Bookmarks** - Named save points within stories
4. **Resume UI** - Menu to start new or resume saved games

This section now assumes Phase 4 compiler output as the runtime input format.

---

## Phase 7: Polish & Optimization (Weeks 4.5-5, ~16 hours)

### Goals
- [ ] Performance tuning and optimization
- [ ] Edge case handling and error recovery
- [ ] Code cleanup and documentation
- [ ] Final testing and validation

### Deliverables (Brief Summary)

1. **Performance Profiling** - Heap usage, latency, refresh optimization
2. **Edge Case Testing** - Long lines, many options, rapid input, errors
3. **Documentation** - Doxygen comments, user guide, code cleanup
4. **Hardware Validation** - Full system testing on Crosspoint device

---

## Rollback Plan

If fundamental issues arise:

1. **Phase 1 Failure**: Revert activity code, keep repo clean
2. **Phase 2 Failure**: Simplify to single-page display (defer pagination)
3. **Phase 3 Failure**: Create standalone VM tester (not integrated yet)
4. **Phase 5+ Failure**: Deploy without runtime integration initially, use dummy data

Default fallback: MVP version with dummy text cycling (Phase 1 + Phase 2).

---

## Risk Mitigation

| Risk | Phase | Probability | Impact | Mitigation |
|------|-------|-------------|--------|-----------|
| Activity integration issues | 1 | Low | High | Early prototype with simple code |
| Text rendering/wrapping bugs | 2 | Medium | Medium | Test with various text lengths |
| Protobuf integration issues | 3 | Medium | High | Early prototyping, fallback to custom parser |
| RAM exhaustion | 3-4 | Low | High | Profile early, implement streaming |
| Display refresh lag | 2-4 | Medium | Medium | Optimize rendering, use PARTIAL refresh |
| SD card I/O failures | 5 | Low | Medium | Error handling, graceful degradation |
| Scope creep | All | Medium | High | Strict adherence to design doc |

---

## Success Criteria

### Phase 1 (Barebones Activity)
- ✅ Activity launches and displays multi-line text
- ✅ Button press cycles through text blocks
- ✅ Compiles without warnings
- ✅ Integration with Activity system works

### Phase 2 (Pagination)
- ✅ Word-wrap works correctly
- ✅ UP/DOWN scrolling responsive
- ✅ Display refresh optimized
- ✅ Visual indicators clear

### Phase 3 (YarnSpinner Core)
- ✅ VM executes bytecode
- ✅ Variables persist
- ✅ Unit tests pass (>80% coverage)
- ✅ No Unreal dependencies

### Phase 4 (Asset Compiler)
- ✅ `.yarnc` + CSV convert to native format deterministically
- ✅ Golden transcript tests match harness output
- ✅ Format schema/version documented

### Phase 5 (Firmware Runtime Integration)
- ✅ Activity uses native runtime to run dialogue
- ✅ Lines and options display correctly
- ✅ Option selection works
- ✅ Branching logic correct

### Phase 6 (Session Management)
- ✅ File browser shows native story asset files
- ✅ Save/load seamless
- ✅ Bookmarks functional
- ✅ No data loss

### Phase 7 (Polish)
- ✅ Performance acceptable
- ✅ All edge cases handled
- ✅ Documentation complete
- ✅ Hardware testing passed

---

## Post-Launch Roadmap (Future)

Not in scope for this project, but potential enhancements:

1. **Advanced Features**
   - Custom Yarn functions (user-defined extensions)
   - Statistics tracking (words read, time spent)
   - Achievements/progress markers

2. **UI Enhancements**
   - Faster page transitions
   - Inline images (for e-ink compatible graphics)
   - Sound effects (if speaker available)

3. **Format Support**
   - YarnC format support (runtime compilation)
   - Other interactive fiction formats (ChoiceScript, TADS)

4. **Cloud Features**
   - Story repository browser
   - Cloud save sync
   - Community sharing

---

## Post-Launch Roadmap (Future)

Not in scope for this project, but potential enhancements:

1. **Advanced Features**
   - Custom Yarn functions (user-defined extensions)
   - Statistics tracking (words read, time spent)
   - Achievements/progress markers

2. **UI Enhancements**
   - Faster page transitions
   - Inline images (for e-ink compatible graphics)
   - Sound effects (if speaker available)

3. **Format Support**
   - YarnC format support (runtime compilation)
   - Other interactive fiction formats (ChoiceScript, TADS)

4. **Cloud Features**
   - Story repository browser
   - Cloud save sync
   - Community sharing

---

## Rollback Plan

If fundamental issues arise:

1. **Phase 1 Failure**: Revert all YarnSpinner code, restore original repo state
2. **Phase 2 Failure**: Simplify presentation (use existing text rendering, defer pagination)
3. **Phase 3 Failure**: Create standalone testing app (not integrated into Crosspoint yet)
4. **Phase 6+ Failure**: Deploy without save/bookmarks initially, add later

Default fallback: MVP version with linear story support only (no branching/options).
