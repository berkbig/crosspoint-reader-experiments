# Interactive Fiction Runtime for Crosspoint - Project Overview

## Executive Summary

This project aims to add a **YarnSpinner Interactive Fiction Runtime** activity to the Crosspoint firmware. This will enable the device to function as a dedicated interactive fiction reader using **device-native story assets generated from YarnSpinner `.yarnc` outputs**.

The implementation will leverage:
- **YarnSpinner Core** (C++ from the Unreal Engine prototype) for dialogue execution and state management
- **Crosspoint Activity System** for UI/UX integration and input handling
- **Crosspoint Display & Rendering** for text presentation on e-ink display
- **Crosspoint Input Manager** for button-based dialogue navigation

## Project Goals

### Primary Goals
1. **Implement a YarnSpinner runtime** that executes a device-native story format derived from YarnSpinner `.yarnc` outputs
2. **Create an interactive fiction activity** integrated into Crosspoint's activity stack
3. **Support dialogue presentation** optimized for e-ink display with pagination
4. **Enable branching dialogue** with button-based option selection

### Non-Goals
- Visual scripting or authoring tools (users supply pre-compiled `.yarn` files)
- Audio/voice support (text-only on e-ink display)
- Dynamic asset loading (embedded resources only)
- Full feature parity with UnrealEngine-based YarnSpinner implementation

## Key Constraints

### Hardware Constraints
- **Limited RAM**: Crosspoint embedded device (~4-8 MB available heap)
- **E-ink Display**: Monochrome, slow refresh (~600ms full refresh)
- **Input**: Limited button controls (usually 4-directional + select)
- **Storage**: SD card for files, but limited working memory

### Firmware Constraints
- **Activity Stack Model**: Single foreground activity with lifecycle (onEnter/onExit/loop/render)
- **Rendering System**: RenderLock-based synchronization, HAL abstraction for display
- **Input Manager**: MappedInputManager with logical button mapping
- **No Direct Unreal Dependencies**: Must strip YarnSpinner Core of all UE-specific code

### Display Constraints
- **Small Screen**: Typically 1024x758 pixels for 6-7" display
- **Monochrome Only**: Text-rendering focus
- **Refresh Optimization**: Minimize partial/full refreshes due to slow update speed
- **Text Wrapping**: Smart text layout with proper pagination

## Scope Summary

### In Scope
- Device-native story execution engine (loaded from compiled runtime assets)
- Variable storage and type system
- Dialogue delivery (Lines, Options, Commands)
- Basic function library (standard operations)
- Button-based navigation (next line, select option)
- Persistent save state (bookmarks, variables)
- File selection/browsing for compiled story asset files

### Out of Scope (Future/External)
- Real-time network features (multiplayer)
- Media embedding (images, audio)
- Advanced graphical UI (buttons, animations)
- Cloud sync or server features
- Runtime protobuf parsing on ESP32 (handled offline during asset compilation)

## Success Criteria

1. ✅ **Can load and execute** a compiled YarnSpinner program
2. ✅ **Renders dialogue** with proper text wrapping for e-ink display
3. ✅ **Handles options** with button-based selection
4. ✅ **Maintains state** across dialogue sessions (bookmarks, variables)
5. ✅ **Integrates seamlessly** with Crosspoint's activity lifecycle
6. ✅ **Respects HAL abstractions** (no direct SDK calls, uses HalStorage/HalDisplay)
7. ✅ **Passes code review** for scope, abstraction, and style compliance
8. ✅ **Runs on-device without protobuf runtime dependency**

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│ InteractiveFiction Activity (extends Activity)              │
│ - Manages lifecycle (onEnter, loop, render, onExit)        │
│ - Integrates with ActivityManager                           │
│ - Handles input via MappedInputManager                      │
└────────────────┬──────────────────────────────────────────┘
                 │
        ┌────────▼────────┐
        │  YarnVM Adapter │ (thin wrapper layer)
        └────────┬────────┘
                 │
        ┌────────▼──────────────────┐
        │ Device-native Story Format │
        │ (compiled offline)         │
        └────────┬──────────────────┘
                │
        ┌────────▼──────────────────┐
        │ Host Asset Compiler        │
        │ (.yarnc -> native format)  │
        └────────┬──────────────────┘
                │
        ┌────────▼──────────────────┐
        │ YarnSpinner Core (C++)     │
        │ - VirtualMachine          │
        │ - Library (functions)     │
        │ - State management        │
        │ - Value system            │
        └────────┬──────────────────┘
                 │
        ┌────────▼───────────────────┐
        │ Variable Storage Backend    │
        │ (In-memory + SD card save) │
        └────────────────────────────┘
```

## File Structure Plan

```
src/
├── activities/
│   └── interactive_fiction/
│       ├── InteractiveFictionActivity.h/cpp
│       ├── YarnSpinnerVM.h/cpp          (adapter/wrapper)
│       ├── YarnVariableStorage.h/cpp    (IVariableStorage impl)
│       ├── YarnLogger.h/cpp             (ILogger impl)
│       └── YarnPresentationManager.h/cpp (rendering/UI)
├── lib/
│   └── yarnspinner_core/
│       ├── Common.h/cpp
│       ├── Value.h/cpp
│       ├── State.h/cpp
│       ├── Library.h/cpp
│       ├── VirtualMachine.h/cpp
│       ├── (runtime parser/VM files for native format)
│       └── (no protobuf runtime dependency on-device)
├── tools/
│   └── yarn_asset_compiler/
│       ├── README.md
│       ├── src/
│       └── fixtures/
└── Research/                             (this directory)
    ├── 01-OVERVIEW.md                   (this file)
    ├── 02-CROSSPOINT-ACTIVITY-DEEP-DIVE.md
    ├── 03-YARNSPINNER-CORE-DEEP-DIVE.md
    └── 04-MILESTONE-PLAN.md
```

## Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|-----------|
| RAM overflow with large programs | Medium | High | Implement streaming bytecode loading, limit variable storage |
| Asset compiler/runtime format mismatch | Medium | High | Lock format version, add golden asset tests |
| Performance on display updates | Medium | Medium | Implement partial refresh, progressive rendering |
| Variable scope mishandling | Medium | Medium | Thorough testing of state transitions |
| Integration friction with Activity system | Low | High | Early prototype of lifecycle integration |

## Next Steps

1. **Phase 1**: Build and validate barebones activity UX foundation
2. **Phase 2**: Improve presentation layer (text wrapping, pagination, rendering)
3. **Phase 3**: Validate YarnSpinner execution with host-side harness
4. **Phase 4**: Build `.yarnc` -> device-native asset compiler
5. **Phase 5**: Integrate native-format runtime in firmware activity
6. **Phase 6**: Add session management
7. **Phase 7**: Testing and optimization

See **04-MILESTONE-PLAN.md** for detailed breakdown.
