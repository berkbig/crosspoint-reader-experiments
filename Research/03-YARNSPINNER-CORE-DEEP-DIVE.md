# YarnSpinner Core C++ - Deep Dive

## Overview

YarnSpinner is a dialogue system that compiles `.yarn` narrative scripts into bytecode (`.yarnc`), which is then executed by a virtual machine. The Unreal Engine prototype provides a portable C++ core that we will adapt for Crosspoint.

**Key Source:** https://github.com/YarnSpinnerTool/YarnSpinner-Unreal-Prototype

## Core Architecture

### 1. Data Types and Values (`Value.h`)

YarnSpinner supports a typed variable system:

```cpp
enum class ValueType {
    Null,      // Undefined/empty value
    Boolean,   // true/false
    Number,    // float (all numeric types unified)
    String     // Text
};

class Value {
    ValueType type;
    union {
        bool boolValue;
        float numberValue;
        // string handled separately
    };
    
    // Type conversions (explicit, may be lossy)
    bool AsBool() const;
    float AsNumber() const;
    std::string AsString() const;
};
```

**Type Coercion Rules:**
- `null` → `false` (bool), `0` (number), `""` (string)
- `true` → `"true"`, `1.0`
- `false` → `"false"`, `0.0`
- Numbers with decimals stringify with decimals
- Parsing strings as numbers: numeric prefix extraction

### 2. YarnC Protobuf Format (Authoring/Build-Time)

The compiled `.yarnc` file is a Protobuf binary containing:

**Key Message Types (auto-generated from `.proto`):**

```protobuf
message Program {
    repeated Node nodes = 1;           // All dialogue nodes
    string name = 2;                   // Program name
    string initial_values = 3;         // JSON of initial variables
}

message Node {
    string name = 1;                   // Node identifier
    repeated Instruction instructions = 2;  // Bytecode
    string tags = 3;                   // Metadata tags
    string source_id = 4;              // Source filename
    string line_number = 5;            // Source line
}

message Instruction {
    enum OpCode {
        JUMP_IF_FALSE = 0;
        JUMP = 1;
        LABEL = 2;
        CALL_FUNCTION = 3;
        PUSH_VALUE = 4;
        POP = 5;
        // ... ~30+ opcodes total
    }
    OpCode opcode = 1;
    string operand_string = 2;  // String operand
    float operand_float = 3;    // Numeric operand
}
```

**Files to Include:**
- `yarn_spinner.pb.h / .cc` - Core protobuf messages (Program, Node, Instruction)
- `compiler_output.pb.h / .cc` - Output format definition

For firmware deployment, this project will **not** parse protobuf on-device. Instead, `.yarnc` and line tables are converted offline into a compact device-native runtime format.

### 3. Virtual Machine (`VirtualMachine.h/cpp`)

The execution engine that interprets bytecode.

**Execution States:**

```cpp
enum ExecutionState {
    STOPPED,                      // No program loaded
    RUNNING,                      // Executing instructions
    DELIVERING_CONTENT,           // About to call Line/Option handler
    WAITING_FOR_CONTINUE,         // Waiting for user to press next
    WAITING_ON_OPTION_SELECTION,  // Waiting for user to pick option
    ERROR                         // Encountered error, cannot continue
};
```

**Key Methods:**

```cpp
class VirtualMachine {
    // Lifecycle
    VirtualMachine(Program program, IVariableStorage& vars, ILogger& logger);
    void SetProgram(Yarn::Program program);
    
    // Execution control
    bool SetNode(const char* nodeName);      // Start a specific node
    bool Continue();                          // Execute until next event (Line/Option/halt)
    ExecutionState GetCurrentExecutionState();
    
    // Option handling
    void SetSelectedOption(int selectedOptionIndex);  // Choose option before Continue()
    
    // Content handlers (callbacks)
    std::function<void(Line&)> LineHandler;
    std::function<void(OptionSet&)> OptionsHandler;
    std::function<void(Command&)> CommandHandler;
    
    // Event handlers
    std::function<void(std::string)> NodeStartHandler;
    std::function<void(std::string)> NodeCompleteHandler;
    std::function<void()> DialogueCompleteHandler;
    
    // Function system
    std::function<bool(std::string)> DoesFunctionExist;
    std::function<int(std::string)> GetExpectedFunctionParamCount;
    std::function<Yarn::Value(std::string, std::vector<Yarn::Value>)> CallFunction;
};
```

**Execution Flow:**

```
SetNode("NodeName")
    ↓
loop: Continue()
    ├─ Execute instructions until event trigger
    ├─ If Line encountered:
    │   ├─ Update state: DELIVERING_CONTENT
    │   ├─ Call LineHandler(line)
    │   └─ State: WAITING_FOR_CONTINUE
    ├─ If OptionSet encountered:
    │   ├─ Update state: DELIVERING_CONTENT
    │   ├─ Call OptionsHandler(options)
    │   └─ State: WAITING_ON_OPTION_SELECTION
    ├─ If Command encountered:
    │   ├─ Call CommandHandler(command)
    │   └─ Continue executing
    ├─ If node complete:
    │   ├─ Call NodeCompleteHandler()
    │   └─ State: STOPPED
    └─ Return execution state
    
Next iteration: call Continue() again based on state
```

### 4. Common Types (`Common.h`)

Data structures for dialogue content:

```cpp
struct Line {
    std::string LineID;                    // Localization key
    std::vector<std::string> Substitutions;  // Values for {0}, {1}, etc.
};

struct Option {
    Line Line;                             // What to display
    int ID;                                // Option identifier
    std::string DestinationNode;           // Where jump leads
    bool IsAvailable;                      // Can user select this?
};

struct OptionSet {
    std::vector<Option> Options;
};

struct Command {
    std::string Text;                      // Custom command string
};

class ILogger {
    virtual void Log(std::string message, Type severity) = 0;
};
```

### 5. Variable Storage (`VirtualMachine.h`)

Abstract interface for persisting variables:

```cpp
class IVariableStorage {
    // Set value by type
    virtual void SetValue(std::string name, bool value) = 0;
    virtual void SetValue(std::string name, float value) = 0;
    virtual void SetValue(std::string name, std::string value) = 0;
    
    // Query values
    virtual bool HasValue(std::string name) = 0;
    virtual Value GetValue(std::string name) = 0;
    
    // Remove value
    virtual void ClearValue(std::string name) = 0;
};
```

**Implementation Requirement:**
We must provide a concrete implementation (e.g., `YarnVariableStorage`) that:
- Stores variables in memory (std::map<std::string, Value>)
- Persists to/from SD card as JSON or binary
- Validates variable names and types

### 6. Function Library (`Library.h`)

Registry for custom functions callable from Yarn scripts:

```cpp
template <typename T>
class Library {
    // Register a function
    template <typename T>
    void AddFunction(std::string name, YarnFunction<T> func, int paramCount);
    
    // Query functions
    template <typename T>
    bool HasFunction(std::string name);
    
    template <typename T>
    FunctionInfo<T> GetFunction(std::string name);
    
    // Standard library
    void LoadStandardLibrary();
};

// Usage in Yarn:
// <<print visited("node1")>>  // Built-in: check if node visited
// <<print random(1, 10)>>     // Built-in: random number
```

**Standard Library Functions:**
- `visited(nodeName)` - Returns true if node has been visited
- `random(min, max)` - Random number
- String functions (concatenation, etc.)
- Math functions

### 7. State Management (`State.h/cpp`)

Internal execution state tracking:

```cpp
class State {
    std::vector<Yarn::Instruction> currentInstructions;
    int programCounter;                    // Instruction pointer
    std::vector<Value> executionStack;     // Operand stack for calculations
    std::map<std::string, Value> variables;
};
```

**Key Operations:**
- Push/pop values from execution stack
- Execute arithmetic/logic opcodes
- Manage program counter for jumps

## Adaptation Strategy for Crosspoint

### What We Keep (Portable Code)

1. ✅ `Common.h` - Data structures (Line, Option, Command)
2. ✅ `Value.h` - Type system
3. ✅ `State.h` - Execution state
4. ✅ `VirtualMachine.h/cpp` - Core execution engine (remove UE deps)
5. ✅ `Library.h/cpp` - Function registry
6. ✅ Protobuf message definitions

### What We Replace

1. ❌ Unreal Engine logging → Custom `ILogger` impl
2. ❌ Unreal Engine memory → Standard C++ STL
3. ❌ Unreal Engine types → Standard C++ types
4. ❌ Unreal Engine `FString` → `std::string`
5. ❌ Unreal Engine containers → `std::vector`, `std::map`

### What We Add

1. ✨ `YarnVariableStorage` - Concrete `IVariableStorage` implementation
2. ✨ `YarnLogger` - Concrete `ILogger` implementation (bridges to Logging.h)
3. ✨ Offline asset compiler (convert `.yarnc` + CSV to device-native format)
4. ✨ Text presentation layer (wrapping, pagination)

## Key Modifications Needed

### 1. Remove Unreal-isms

**Search and Replace:**
- `YARNSPINNER_API` → (remove; no longer needed for exports)
- `FString` → `std::string`
- `TArray<T>` → `std::vector<T>`
- `TMap<K,V>` → `std::map<K,V>`
- `TSharedPtr<T>` → `std::shared_ptr<T>` or `std::unique_ptr<T>`
- `check()` assertions → `assert()` or custom error handling
- `UE_LOG` → Call `ILogger` interface

### 2. Protobuf Integration

**Options:**
1. Parse protobuf directly on device (highest runtime overhead)
2. Parse protobuf in host tooling and emit native assets (recommended)
3. Maintain custom parser/runtime for raw `.yarnc` (more work, least desirable)

**Recommended:** Parse `.yarnc` in host tooling, ship native assets to firmware.

### 3. Thread Safety

YarnSpinner Core is **not thread-safe** by default:
- Assumes single-threaded execution
- **OK for Crosspoint:** Activity loop is single-threaded; render thread doesn't access VM

**Requirement:** Only call VM methods from activity loop, never from render thread.

## Integration Interface (`YarnSpinnerVM.h` - NEW)

Recommended adapter layer (thin wrapper):

```cpp
class YarnSpinnerVM {
    std::unique_ptr<Yarn::VirtualMachine> vm;
    std::unique_ptr<Yarn::IVariableStorage> varStorage;
    std::unique_ptr<Yarn::ILogger> logger;
    
public:
    // Initialize with .yarn file path
    bool LoadProgram(const std::string& filePath);
    
    // Execution
    bool SetNode(const std::string& nodeName);
    bool Continue();
    ExecutionState GetState();
    
    // Option handling
    void SelectOption(int index);
    
    // Access current content
    const Line* GetCurrentLine() const;
    const OptionSet* GetCurrentOptions() const;
    
    // Variable access (for save/load)
    std::map<std::string, std::string> GetAllVariables();
    void SetVariables(const std::map<std::string, std::string>& vars);
};
```

## Performance Considerations

### Memory Profile
- VirtualMachine itself: ~1KB
- Program (bytecode): Depends on file (typically 10-100KB for medium story)
- Variable storage: 1 variable ≈ 50 bytes (name + value)
- Execution stack: Typically <10 values active

**Typical Total:** 50-200KB for a complete interactive fiction experience

### Execution Performance
- Each `Continue()` call: ~1-5ms (instruction execution, depends on complexity)
- **No** blocking I/O during execution (SD card read happens at load time)
- Ideal for real-time button responsiveness

### Display Performance
- Line delivery → Render update → ~600ms display refresh
- User button press → Response → Next render (interactive responsiveness acceptable)

## Debugging Aids

### Logging Points
- Node transitions (start/complete)
- Option selection
- Variable changes
- Error states
- Execution time metrics

### State Inspection
- Print current node name
- Dump variable values
- List available options

## Reference Implementation Examples

### Example: Simple Linear Story

```yarn
title: Start
---
This is the first line.
-> Second
===

title: Second
---
This is the second line.
-> End
===

title: End
---
The story is complete.
===
```

**Compiled → VirtualMachine Execution:**
1. SetNode("Start")
2. Continue() → delivers "This is the first line" + jump to "Second"
3. Continue() → sets node to "Second", delivers "This is the second line" + jump
4. Continue() → sets node to "End", delivers "The story is complete"
5. Continue() → NodeCompleteHandler(), execution ends

### Example: Branching Story

```yarn
title: Choice
---
You see two paths.
    -> Take left path
        Left is cold and dark.
    -> Take right path
        Right is warm and bright.
===
```

**Execution:**
1. SetNode("Choice")
2. Continue() → OptionsHandler called with 2 options
3. State = WAITING_ON_OPTION_SELECTION
4. User selects option 0 (left)
5. SetSelectedOption(0), then Continue()
6. Delivers "Left is cold and dark"

## Files Location in Source

- Public headers: `Source/YarnSpinner/Public/YarnSpinnerCore/`
- Implementation: `Source/YarnSpinner/Private/YarnSpinnerCore/`
- Protobuf definitions: `Source/YarnSpinner/Private/YarnSpinnerCore/` (`.proto` files generate `.pb.h/cc`)

## Critical Porting Issues

| Issue | Severity | Solution |
|-------|----------|----------|
| Unreal-specific macros | HIGH | Search & replace all UE types |
| Protobuf version mismatch | MEDIUM | Keep protobuf in host toolchain only; pin YarnSpinner/protobuf versions |
| Memory allocation patterns | LOW | Use STL allocators; profile heap usage |
| Missing #include guards for Unreal | HIGH | Remove UE includes, add C++ standard ones |
| Thread safety assumptions | LOW | Document: VM access from loop only |
