#pragma once

#include <cstdint>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

class YsaRuntime {
 public:
  struct OptionView {
    std::string text;
    bool selected = false;
  };

  bool loadFromStorage(const char* path);
  bool start(const std::string& nodeName = "Start");
  bool chooseSelectedOption();
  bool moveSelection(int delta);

  bool hasStory() const { return !nodes.empty() || !nodeIndex.empty(); }
  bool isWaitingForChoice() const { return waitingForChoice; }
  bool isFinished() const { return finished; }
  bool hasError() const { return errored; }
  const std::string& getError() const { return errorMessage; }

  const std::vector<std::string>& getTranscript() const { return transcript; }
  std::vector<OptionView> getVisibleOptions() const;

 private:
  enum class ValueType : uint8_t { Float, Bool, String };
  enum class OperandType : uint8_t { U16 = 0, Bool = 1, String = 2, Float = 3 };

  struct Value {
    ValueType type = ValueType::Float;
    float floatValue = 0.0f;
    bool boolValue = false;
    std::string stringValue;

    static Value fromFloat(float v);
    static Value fromBool(bool v);
    static Value fromString(std::string v);
    bool asBool() const;
    float asFloat() const;
    std::string asString() const;
  };

  struct Operand {
    OperandType type = OperandType::U16;
    uint16_t u16Value = 0;
    bool boolValue = false;
    float floatValue = 0.0f;
    std::string stringValue;
  };

  struct Instruction {
    uint8_t opcode = 0;
    std::vector<Operand> operands;
  };

  struct Node {
    std::vector<Instruction> instructions;
  };

  struct Frame {
    std::string nodeName;
    int ip = 0;
  };

  struct RuntimeState {
    std::map<std::string, Value> variables;
    std::set<std::string> visitedNodes;
    std::map<std::string, int> visitCounts;
    std::mt19937 rng{std::random_device{}()};
  };

  struct OptionChoice {
    std::string resolvedText;
    int destination = -1;
    bool available = true;
  };

  struct SaliencyCandidate {
    std::string id;
    int complexityScore = 0;
    int destination = -1;
  };

  struct NodeIndex {
    size_t fileOffset = 0;
    uint16_t instructionCount = 0;
  };

  uint16_t version = 0;
  std::map<std::string, Node> nodes;  // Parsed nodes cache
  std::map<std::string, NodeIndex> nodeIndex;  // All nodes: offset + count
  std::vector<uint8_t> fileData;  // Raw file buffer for on-demand parsing
  std::map<std::string, std::string> lineTable;
  std::map<std::string, Value> initialValues;

  RuntimeState state;
  std::vector<Frame> frames;
  std::vector<Value> stack;
  std::vector<OptionChoice> pendingOptions;
  std::vector<SaliencyCandidate> saliencyCandidates;
  std::vector<std::string> transcript;

  bool waitingForChoice = false;
  bool finished = false;
  bool errored = false;
  int selectedOption = -1;
  std::string errorMessage;

  bool runUntilPause();
  bool executeInstruction();
  bool setError(const std::string& msg);
  void markVisited(const std::string& nodeName);
  void resetRuntimeState();

  bool readU8(const std::vector<uint8_t>& data, size_t& offset, uint8_t& out) const;
  bool readU16(const std::vector<uint8_t>& data, size_t& offset, uint16_t& out) const;
  bool readU32(const std::vector<uint8_t>& data, size_t& offset, uint32_t& out) const;
  bool readF32(const std::vector<uint8_t>& data, size_t& offset, float& out) const;
  bool readString(const std::vector<uint8_t>& data, size_t& offset, std::string& out) const;

  bool loadFromBuffer(const std::vector<uint8_t>& data);
  bool ensureNodeLoaded(const std::string& nodeName);

  const Operand* requireOperand(const Instruction& ins, size_t index, OperandType type);
  std::vector<Value> popSubstitutions(int count);
  std::string lookupLine(const std::string& lineId) const;
  std::string applySubstitutions(const std::string& text, const std::vector<Value>& substitutions) const;
  Value callFunction(std::string name, std::vector<Value> args);
  bool evaluateSmartVariable(const std::string& variableName, Value& outValue, std::set<std::string>& activeNodes);
  int firstAvailableOption() const;
};
