# Crosspoint Activity System - Deep Dive

## Overview

The Crosspoint firmware follows an Android-like Activity pattern where each distinct UI screen or feature is represented as an `Activity` object. The `ActivityManager` maintains a stack of activities and ensures only one activity is active (in the foreground) at a time.

## Core Activity Lifecycle

### Activity Base Class

Located: `src/activities/Activity.h` and `src/activities/Activity.cpp`

**Key Members:**
```cpp
class Activity {
    std::string name;                          // Activity identifier
    GfxRenderer& renderer;                     // Rendering engine (not owned)
    MappedInputManager& mappedInput;           // Input handling (not owned)
    ActivityResultHandler resultHandler;       // Callback when activity finishes
    ActivityResult result;                     // Result to pass to parent
};
```

**Lifecycle Methods (Virtual):**
- `onEnter()` - Called when activity becomes foreground; initialize resources, trigger initial render
- `loop()` - Called repeatedly by ActivityManager; process input and logic
- `render(RenderLock&&)` - Called by dedicated render task; update display
- `onExit()` - Called when activity is being removed; cleanup resources

**Control Methods:**
- `requestUpdate(bool immediate = false)` - Request a render pass (immediate or deferred)
- `requestUpdateAndWait()` - Block until render completes
- `startActivityForResult()` - Launch a sub-activity and get result via callback
- `finish()` - Pop this activity from the stack, return to previous
- `setResult(ActivityResult&&)` - Pass data back to parent activity

### Multi-Threading Model

Crosspoint uses a **two-task model**:

1. **Main Task** (`loop()`)
   - Runs the activity's `loop()` method
   - Handles state updates and logic
   - NOT responsible for rendering
   - Calls `requestUpdate()` when render needed

2. **Render Task** (dedicated, high priority)
   - Continuously monitors for render requests
   - Calls activity's `render(RenderLock&&)` method
   - Updates the e-ink display
   - Uses `RenderLock` for synchronization with main task

**Synchronization:**
- `RenderLock` is a RAII wrapper around a FreeRTOS semaphore
- Ensures exclusive access to renderer during activity transitions
- `requestUpdateAndWait()` blocks main task until render completes

## ActivityManager Architecture

Located: `src/activities/ActivityManager.h`

**Key Responsibilities:**
- Stack management (push/pop activities)
- Lifecycle coordination (onEnter, loop, render, onExit)
- Input event dispatch to active activity
- Auto-sleep management (preventAutoSleep hook)
- Pending activity queue (deferred start)

**Stack-Based Navigation:**
```
Frame N:   Current Activity (in foreground)
Frame N-1: Previous Activity (on stack, inactive)
Frame N-2: Previous Activity (on stack, inactive)
```

**Activity Transitions:**
1. Push: `current → stack`, `new → current`
2. Pop: `current → deleted`, `stack.top() → current`
3. Replace: `current → deleted`, `new → current` (stack unchanged)

## Rendering System

Located: `src/GfxRenderer.h`

**Key Pattern: HAL Abstraction**

The firmware uses a Hardware Abstraction Layer (HAL) to decouple display operations:
- No direct SDK calls in activities
- Routing through `HalDisplay` (storage, GPIO, etc.)
- `GfxRenderer` provides high-level drawing APIs
- Activities use `UITheme` for consistent styling

**Typical Rendering Workflow:**
```cpp
void MyActivity::render(RenderLock&& lock) {
    auto& canvas = renderer.getCanvas();  // Get drawing surface
    
    canvas.clear();                       // Clear background
    canvas.drawText(10, 20, "Hello");     // Draw text
    // More drawing operations...
    
    renderer.requestRefresh(RefreshMode::FULL);  // Trigger display update
}
```

**Rendering Modes:**
- `FULL`: Full screen refresh (slow, ~600ms on 6" e-ink)
- `PARTIAL`: Partial area refresh (faster, ~200ms, may cause artifacts)
- `WAVEFORM`: Custom waveform (advanced, rare)

## Input Handling

Located: `src/MappedInputManager.h`

**Logical Button Mapping:**
- Hardware GPIO → Logical button names
- Configuration in `CrossPointSettings`
- Examples: UP, DOWN, LEFT, RIGHT, SELECT, BACK, HOME

**Input Flow:**
```
Hardware GPIO Interrupt
    ↓
MappedInputManager (debounce, translate)
    ↓
Activity::loop() checks via `mappedInput.isPressed(Button::SELECT)`
    ↓
Activity state changes, requestUpdate() called
```

**Pattern for Input Handling:**
```cpp
void MyActivity::loop() {
    if (mappedInput.isPressed(Button::SELECT)) {
        // Handle select button
        onSelectPressed();
        requestUpdate();
    }
}
```

## Example: TxtReaderActivity

For reference, `TxtReaderActivity` (located `src/activities/reader/TxtReaderActivity.h`) demonstrates:
- Complex UI with pagination
- Text rendering with wrapping
- Button navigation (previous/next page)
- Status bar integration
- Progress persistence to SD card
- Cache management for performance

**Key Features We'll Borrow:**
- Streaming file loading (offset-based pagination)
- Cache validation (re-index on setting changes)
- Progress saving to persistent storage
- Page calculation and viewport management

## Integration Points for Interactive Fiction Activity

### 1. Activity Registration

Interactive Fiction Activity must be registered with the ActivityManager (likely in home/FileBrowserActivity where users select files):

```cpp
auto ifActivity = std::make_unique<InteractiveFictionActivity>(
    renderer, mappedInput, filePath
);
activityManager.startActivityForResult(
    std::move(ifActivity),
    [](const ActivityResult& result) {
        // Handle result when IF activity finishes
    }
);
```

### 2. Rendering Pattern

Our activity must respect the render lock pattern:

```cpp
void InteractiveFictionActivity::render(RenderLock&& lock) {
    // Draw current dialogue/options
    // Respect canvas size (1024x758 typical)
    // Handle text wrapping for monospace font
    renderer.requestRefresh(RefreshMode::PARTIAL);
}
```

### 3. Loop Pattern

Process input and advance dialogue state:

```cpp
void InteractiveFictionActivity::loop() {
    // Check for button presses
    if (mappedInput.isPressed(Button::SELECT)) {
        vm.setSelectedOption(selectedOptionIndex);
        bool shouldContinue = vm.Continue();
        if (!shouldContinue) {
            finish();  // Close activity
        }
        requestUpdate();
    }
    // Navigation between options...
}
```

### 4. State Persistence

Save/load via HAL:

```cpp
void InteractiveFictionActivity::saveState() {
    // Use HalStorage to write state to SD card
    // Include: current node, variables, bookmarks
}

void InteractiveFictionActivity::loadState() {
    // Use HalStorage to read saved state from SD card
}
```

## Activity Lifecycle for IF Runtime

**Typical Session:**

1. **onEnter()**
   - Load `.yarn` file from SD card
   - Initialize YarnSpinner VirtualMachine
   - Load saved variables (if resuming)
   - Set initial node
   - Call `vm.Continue()` to deliver first content
   - `requestUpdate()`

2. **loop()** (repeated)
   - Check for input (UP/DOWN to select option, SELECT to confirm)
   - Update selectedOptionIndex
   - Check execution state of VM
   - If WAITING_ON_OPTION_SELECTION: handle option selection
   - If WAITING_FOR_CONTINUE: wait for button press
   - Call `vm.Continue()` as needed
   - `requestUpdate()` when state changes

3. **render(RenderLock&&)**
   - Clear canvas
   - Render current line text with wrapping
   - Render available options (with selection highlight)
   - Render status bar (current node, progress)
   - Render instructions (e.g., "Press SELECT to continue")

4. **onExit()**
   - Save current state to SD card
   - Free YarnSpinner resources
   - Close file handles

## Key Constraints for IF Activity

### Memory
- Each `.yarn` file must fit in available heap
- Variable storage must be bounded
- Consider streaming/chunking if files are large

### Display
- E-ink update latency (~600ms full refresh)
- Minimize refresh count (don't refresh on every VM instruction)
- Use PARTIAL refresh for option selection highlighting

### Input
- Only 4 buttons typically (UP/DOWN/SELECT/BACK)
- Must navigate between options with UP/DOWN
- SELECT confirms choice
- BACK exits activity

### Integration
- Must not bypass Activity lifecycle
- Use HAL for all storage/display/input
- Follow UITheme for visual consistency
- Cooperate with ActivityManager's auto-sleep

## Relevant Files to Review

1. **Activity System**
   - `src/activities/Activity.h/cpp` - Base class
   - `src/activities/ActivityManager.h/cpp` - Manager
   - `src/activities/ActivityResult.h` - Result passing

2. **Rendering**
   - `src/GfxRenderer.h` - Drawing API
   - `src/components/UITheme.h` - Styling
   - `src/RenderLock.h` - Synchronization

3. **Input**
   - `src/MappedInputManager.h` - Button handling
   - `src/CrossPointSettings.h` - Button mapping config

4. **Reference Implementation**
   - `src/activities/reader/TxtReaderActivity.h/cpp` - Text-based activity
   - `src/activities/home/FileBrowserActivity.h/cpp` - File selection
   - `src/activities/util/ConfirmationActivity.h/cpp` - Simple UI activity

## Design Patterns to Follow

1. **Use References, Not Pointers** for injected dependencies (renderer, mappedInput)
2. **RAII for Resource Cleanup** (files, memory allocations)
3. **Deferred Rendering** (call requestUpdate(), don't render directly)
4. **Logical Button Names** (not hardware-specific GPIO indices)
5. **HAL for Storage** (not raw file I/O)
6. **Result Callbacks** for sub-activities (not global state)
