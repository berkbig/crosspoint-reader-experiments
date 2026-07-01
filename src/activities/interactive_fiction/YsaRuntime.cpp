#include "YsaRuntime.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {
constexpr uint8_t OP_RUN_LINE = 1;
constexpr uint8_t OP_RUN_COMMAND = 2;
constexpr uint8_t OP_ADD_OPTION = 3;
constexpr uint8_t OP_SHOW_OPTIONS = 4;
constexpr uint8_t OP_JUMP_TO = 5;
constexpr uint8_t OP_STOP = 6;
constexpr uint8_t OP_RUN_NODE = 7;
constexpr uint8_t OP_JUMP_IF_FALSE = 8;
constexpr uint8_t OP_PEEK_AND_JUMP = 9;
constexpr uint8_t OP_RETURN = 10;
constexpr uint8_t OP_POP = 11;
constexpr uint8_t OP_PUSH_STRING = 12;
constexpr uint8_t OP_PUSH_FLOAT = 13;
constexpr uint8_t OP_PUSH_BOOL = 14;
constexpr uint8_t OP_PUSH_VARIABLE = 15;
constexpr uint8_t OP_STORE_VARIABLE = 16;
constexpr uint8_t OP_CALL_FUNCTION = 17;
constexpr uint8_t OP_DETOUR_TO_NODE = 18;
constexpr uint8_t OP_PEEK_AND_DETOUR = 19;
constexpr uint8_t OP_PEEK_AND_RUN_NODE = 20;
constexpr uint8_t OP_ADD_SALIENCY_CANDIDATE = 21;
constexpr uint8_t OP_ADD_SALIENCY_CANDIDATE_FROM_NODE = 22;
constexpr uint8_t OP_SELECT_SALIENCY_CANDIDATE = 23;

constexpr size_t MAX_TRANSCRIPT_LINES = 200;
constexpr size_t MAX_CACHED_NODES = 32;
constexpr size_t MAX_CACHED_NODE_BYTES = 24 * 1024;

const std::map<std::string, int> kFunctionMinArity = {
    {"Add", 2},          {"Minus", 2},          {"Multiply", 2},      {"Divide", 2},       {"Modulo", 2},
    {"EqualTo", 2},      {"NotEqualTo", 2},     {"LessThan", 2},      {"GreaterThan", 2},  {"LessThanOrEqualTo", 2},
    {"GreaterThanOrEqualTo", 2},                {"And", 2},           {"Or", 2},           {"Xor", 2},
    {"Not", 1},          {"UnaryMinus", 1},     {"visited", 1},       {"visited_count", 1},
    {"random", 0},       {"random_range", 2},   {"dice", 1},          {"round", 1},        {"round_places", 2},
    {"floor", 1},        {"ceil", 1},           {"inc", 1},           {"dec", 1},          {"decimal", 1},
    {"int", 1},          {"string", 1},         {"number", 1},        {"bool", 1},         {"yn", 1},
    {"format_invariant", 1},
};

float parseFloatOrZero(const std::string& s) {
  char* end = nullptr;
  const float value = strtof(s.c_str(), &end);
  return (end != s.c_str()) ? value : 0.0f;
}
}  // namespace

YsaRuntime::Value YsaRuntime::Value::fromFloat(float v) {
  Value out;
  out.type = ValueType::Float;
  out.floatValue = v;
  return out;
}

YsaRuntime::Value YsaRuntime::Value::fromBool(bool v) {
  Value out;
  out.type = ValueType::Bool;
  out.boolValue = v;
  return out;
}

YsaRuntime::Value YsaRuntime::Value::fromString(std::string v) {
  Value out;
  out.type = ValueType::String;
  out.stringValue = std::move(v);
  return out;
}

bool YsaRuntime::Value::asBool() const {
  switch (type) {
    case ValueType::Bool:
      return boolValue;
    case ValueType::Float:
      return floatValue != 0.0f;
    case ValueType::String:
      return !stringValue.empty();
  }
  return false;
}

float YsaRuntime::Value::asFloat() const {
  switch (type) {
    case ValueType::Float:
      return floatValue;
    case ValueType::Bool:
      return boolValue ? 1.0f : 0.0f;
    case ValueType::String:
      return parseFloatOrZero(stringValue);
  }
  return 0.0f;
}

std::string YsaRuntime::Value::asString() const {
  switch (type) {
    case ValueType::String:
      return stringValue;
    case ValueType::Bool:
      return boolValue ? "true" : "false";
    case ValueType::Float: {
      char buffer[32] = {0};
      const float floored = floorf(floatValue);
      if (fabsf(floatValue - floored) < 0.0001f) {
        snprintf(buffer, sizeof(buffer), "%ld", static_cast<long>(floored));
      } else {
        snprintf(buffer, sizeof(buffer), "%g", static_cast<double>(floatValue));
      }
      return buffer;
    }
  }
  return {};
}

bool YsaRuntime::loadFromStorage(const char* path) {
  nodes.clear();
  nodeLastUseTick.clear();
  nodeEstimatedBytes.clear();
  cachedNodeBytes = 0;
  nextNodeUseTick = 1;
  nodeIndex.clear();
  lineTable.clear();
  initialValues.clear();
  transcript.clear();
  storagePath = path;

  HalFile file;
  if (!Storage.openFileForRead("IF", path, file) || !file) {
    return setError(std::string("Failed to open story: ") + path);
  }

  const size_t fileSize = file.fileSize();
  if (fileSize == 0 || fileSize > static_cast<size_t>(std::numeric_limits<uint16_t>::max()) * 16) {
    return setError("Invalid story size");
  }

  auto readExactFile = [&](void* dst, size_t count) -> bool {
    if (count == 0) {
      return true;
    }
    const int n = file.read(dst, count);
    return n == static_cast<int>(count);
  };

  auto readU8File = [&](uint8_t& out) -> bool { return readExactFile(&out, 1); };
  auto readU16File = [&](uint16_t& out) -> bool { return readExactFile(&out, 2); };
  auto readU32File = [&](uint32_t& out) -> bool { return readExactFile(&out, 4); };
  auto readF32File = [&](float& out) -> bool { return readExactFile(&out, 4); };
  auto readStringFile = [&](std::string& out) -> bool {
    uint16_t len = 0;
    if (!readU16File(len)) {
      return false;
    }
    out.assign(len, '\0');
    if (len == 0) {
      return true;
    }
    return readExactFile(out.data(), len);
  };

  uint8_t magic[4] = {};
  if (!readExactFile(magic, sizeof(magic))) {
    return setError("Story too small");
  }
  if (!(magic[0] == 'Y' && magic[1] == 'S' && magic[2] == 'A' && magic[3] == '1')) {
    return setError("Invalid YSA magic");
  }

  uint16_t nodeCount = 0;
  uint16_t initialValueCount = 0;
  uint32_t lineCount = 0;
  if (!readU16File(version) || !readU16File(nodeCount) || !readU32File(lineCount)) {
    return setError("Invalid header");
  }
  if (version == 2) {
    if (!readU16File(initialValueCount)) {
      return setError("Invalid initial value header");
    }
  } else if (version != 1) {
    return setError("Unsupported YSA version");
  }

  // Build index without loading full file into memory.
  for (uint16_t i = 0; i < nodeCount; ++i) {
    std::string nodeName;
    uint16_t instructionCount = 0;
    if (!readStringFile(nodeName) || !readU16File(instructionCount)) {
      return setError("Invalid node header");
    }

    NodeIndex idx;
    idx.fileOffset = file.position();  // first instruction
    idx.instructionCount = instructionCount;
    nodeIndex[nodeName] = idx;

    for (uint16_t j = 0; j < instructionCount; ++j) {
      uint16_t instructionSize = 0;
      if (!readU16File(instructionSize) || !file.seekCur(instructionSize)) {
        return setError("Invalid instruction size");
      }
    }
  }

  // Parse line table
  for (uint32_t i = 0; i < lineCount; ++i) {
    std::string lineId;
    std::string text;
    if (!readStringFile(lineId) || !readStringFile(text)) {
      return setError("Invalid line table");
    }
    lineTable[lineId] = text;
  }

  // Parse initial values
  for (uint16_t i = 0; i < initialValueCount; ++i) {
    std::string varName;
    uint8_t valueType = 0;
    if (!readStringFile(varName) || !readU8File(valueType)) {
      return setError("Invalid initial value");
    }

    Value value = Value::fromFloat(0.0f);
    switch (valueType) {
      case 0: {
        float f = 0.0f;
        if (!readF32File(f)) return setError("Invalid float initial value");
        value = Value::fromFloat(f);
        break;
      }
      case 1: {
        uint8_t b = 0;
        if (!readU8File(b)) return setError("Invalid bool initial value");
        value = Value::fromBool(b != 0);
        break;
      }
      case 2: {
        std::string s;
        if (!readStringFile(s)) return setError("Invalid string initial value");
        value = Value::fromString(s);
        break;
      }
      default:
        return setError("Invalid initial value type");
    }
    initialValues[varName] = std::move(value);
  }

  errored = false;
  errorMessage.clear();
  return true;
}

bool YsaRuntime::ensureNodeLoaded(const std::string& nodeName, const std::set<std::string>* extraPinnedNodes) {
  // If already in cache, we're done
  if (nodes.find(nodeName) != nodes.end()) {
    touchNode(nodeName);
    trimNodeCache(extraPinnedNodes, &nodeName);
    return true;
  }

  // Must be in index for us to load it
  auto indexIt = nodeIndex.find(nodeName);
  if (indexIt == nodeIndex.end()) {
   return setError(std::string("Node not found: ") + nodeName);
  }

  const NodeIndex& idx = indexIt->second;

  // Reopen file to read just this node
  HalFile file;
  if (!Storage.openFileForRead("IF", storagePath.c_str(), file) || !file) {
   return setError(std::string("Failed to reopen story: ") + storagePath);
  }

  if (!file.seek(idx.fileOffset)) {
   return setError("Failed to seek to node");
  }

  auto readExactFile = [&](void* dst, size_t count) -> bool {
   if (count == 0) {
     return true;
   }
   const int n = file.read(dst, count);
   return n == static_cast<int>(count);
  };

  auto readU16File = [&](uint16_t& out) -> bool { return readExactFile(&out, 2); };

  // Parse just this node's instructions
  Node node;
  node.instructions.reserve(idx.instructionCount);

  for (uint16_t j = 0; j < idx.instructionCount; ++j) {
   uint16_t instructionSize = 0;
   if (!readU16File(instructionSize)) {
     return setError("Invalid instruction size during lazy load");
   }

   std::vector<uint8_t> instructionData(instructionSize);
   if (!readExactFile(instructionData.data(), instructionSize)) {
     return setError("Invalid instruction payload during lazy load");
   }

   size_t offset = 0;

   Instruction ins;
   uint8_t operandCount = 0;
   if (!readU8(instructionData, offset, ins.opcode) || !readU8(instructionData, offset, operandCount)) {
     return setError("Invalid instruction header during lazy load");
   }

   for (uint8_t k = 0; k < operandCount; ++k) {
     uint8_t typeRaw = 0;
     if (!readU8(instructionData, offset, typeRaw)) {
       return setError("Invalid operand type during lazy load");
     }
     Operand operand;
     operand.type = static_cast<OperandType>(typeRaw);
     switch (operand.type) {
       case OperandType::U16:
         if (!readU16(instructionData, offset, operand.u16Value)) return setError("Invalid u16 operand during lazy load");
         break;
       case OperandType::Bool: {
         uint8_t value = 0;
         if (!readU8(instructionData, offset, value)) return setError("Invalid bool operand during lazy load");
         operand.boolValue = value != 0;
         break;
       }
       case OperandType::String:
         if (!readString(instructionData, offset, operand.stringValue)) return setError("Invalid string operand during lazy load");
         break;
       case OperandType::Float:
         if (!readF32(instructionData, offset, operand.floatValue)) return setError("Invalid float operand during lazy load");
         break;
       default:
         return setError("Unknown operand type during lazy load");
     }
     ins.operands.push_back(std::move(operand));
   }

   if (offset != instructionData.size()) {
     return setError("Instruction length mismatch during lazy load");
   }
   node.instructions.push_back(std::move(ins));
  }

  // Cache the parsed node
  const size_t estimatedBytes = estimateNodeBytes(node);
  nodes[nodeName] = std::move(node);
  nodeEstimatedBytes[nodeName] = estimatedBytes;
  cachedNodeBytes += estimatedBytes;
  touchNode(nodeName);
  trimNodeCache(extraPinnedNodes, &nodeName);
  return true;
}

void YsaRuntime::touchNode(const std::string& nodeName) {
  nodeLastUseTick[nodeName] = nextNodeUseTick++;
}

size_t YsaRuntime::estimateNodeBytes(const Node& node) const {
  size_t total = sizeof(Node);
  total += node.instructions.capacity() * sizeof(Instruction);
  for (const Instruction& ins : node.instructions) {
    total += ins.operands.capacity() * sizeof(Operand);
    for (const Operand& op : ins.operands) {
      total += op.stringValue.capacity();
    }
  }
  return total;
}

void YsaRuntime::trimNodeCache(const std::set<std::string>* extraPinnedNodes, const std::string* justLoadedNode) {
  if (nodes.size() <= MAX_CACHED_NODES && cachedNodeBytes <= MAX_CACHED_NODE_BYTES) {
    return;
  }

  std::set<std::string> pinned;
  if (justLoadedNode) {
    pinned.insert(*justLoadedNode);
  }
  for (const Frame& frame : frames) {
    pinned.insert(frame.nodeName);
  }
  if (extraPinnedNodes) {
    pinned.insert(extraPinnedNodes->begin(), extraPinnedNodes->end());
  }

  while (nodes.size() > MAX_CACHED_NODES || cachedNodeBytes > MAX_CACHED_NODE_BYTES) {
    auto candidateIt = nodes.end();
    uint32_t oldestTick = std::numeric_limits<uint32_t>::max();
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
      if (pinned.count(it->first) > 0) {
        continue;
      }
      const auto tickIt = nodeLastUseTick.find(it->first);
      const uint32_t tick = (tickIt != nodeLastUseTick.end()) ? tickIt->second : 0;
      if (tick < oldestTick) {
        oldestTick = tick;
        candidateIt = it;
      }
    }

    if (candidateIt == nodes.end()) {
      return;
    }

    const auto bytesIt = nodeEstimatedBytes.find(candidateIt->first);
    if (bytesIt != nodeEstimatedBytes.end()) {
      if (cachedNodeBytes >= bytesIt->second) {
        cachedNodeBytes -= bytesIt->second;
      } else {
        cachedNodeBytes = 0;
      }
      nodeEstimatedBytes.erase(bytesIt);
    }
    nodeLastUseTick.erase(candidateIt->first);
    nodes.erase(candidateIt);
  }
}

bool YsaRuntime::start(const std::string& nodeName) {
  if (!hasStory()) {
    return setError("No story loaded");
  }

  if (!ensureNodeLoaded(nodeName)) {
    return false;
  }

  resetRuntimeState();
  frames.push_back({nodeName, 0});
  return runUntilPause();
}

bool YsaRuntime::chooseSelectedOption() {
  if (!waitingForChoice) {
    return true;
  }
  if (frames.empty()) {
    return setError("No active frame for option selection");
  }
  if (selectedOption < 0 || selectedOption >= static_cast<int>(pendingOptions.size())) {
    return setError("Invalid selected option index");
  }
  const OptionChoice& option = pendingOptions[static_cast<size_t>(selectedOption)];
  stack.push_back(Value::fromFloat(static_cast<float>(option.destination)));
  stack.push_back(Value::fromBool(true));
  pendingOptions.clear();
  waitingForChoice = false;
  selectedOption = -1;
  // Start a fresh page after each confirmed choice.
  transcript.clear();
  frames.back().ip += 1;
  return runUntilPause();
}

bool YsaRuntime::moveSelection(int delta) {
  if (!waitingForChoice) {
    return false;
  }

  std::vector<int> availableIndices;
  for (size_t i = 0; i < pendingOptions.size(); ++i) {
    if (pendingOptions[i].available) {
      availableIndices.push_back(static_cast<int>(i));
    }
  }
  if (availableIndices.empty()) {
    return false;
  }

  int currentPos = 0;
  for (size_t i = 0; i < availableIndices.size(); ++i) {
    if (availableIndices[i] == selectedOption) {
      currentPos = static_cast<int>(i);
      break;
    }
  }

  const int size = static_cast<int>(availableIndices.size());
  const int nextPos = (currentPos + delta + size) % size;
  selectedOption = availableIndices[static_cast<size_t>(nextPos)];
  return true;
}

std::vector<YsaRuntime::OptionView> YsaRuntime::getVisibleOptions() const {
  std::vector<OptionView> out;
  for (size_t i = 0; i < pendingOptions.size(); ++i) {
    const auto& option = pendingOptions[i];
    if (!option.available) {
      continue;
    }
    out.push_back({option.resolvedText, static_cast<int>(i) == selectedOption});
  }
  return out;
}

bool YsaRuntime::runUntilPause() {
  while (!frames.empty() && !waitingForChoice && !finished && !errored) {
    if (!executeInstruction()) {
      return false;
    }
  }

  if (!errored && frames.empty() && !finished) {
    finished = true;
  }
  return !errored;
}

bool YsaRuntime::executeInstruction() {
  if (frames.empty()) {
    return true;
  }

  Frame& frame = frames.back();
  const auto nodeIt = nodes.find(frame.nodeName);
  if (nodeIt == nodes.end()) {
    return setError(std::string("Node not found: ") + frame.nodeName);
  }
  touchNode(frame.nodeName);
  const Node& node = nodeIt->second;

  if (frame.ip < 0 || frame.ip >= static_cast<int>(node.instructions.size())) {
    markVisited(frame.nodeName);
    frames.pop_back();
    return true;
  }

  const Instruction& ins = node.instructions[static_cast<size_t>(frame.ip)];
  switch (ins.opcode) {
    case OP_RUN_LINE: {
      const Operand* lineId = requireOperand(ins, 0, OperandType::String);
      const Operand* subCount = requireOperand(ins, 1, OperandType::U16);
      if (!lineId || !subCount) return false;
      const auto substitutions = popSubstitutions(subCount->u16Value);
      const std::string text = applySubstitutions(lookupLine(lineId->stringValue), substitutions);
      transcript.push_back(text);
      if (transcript.size() > MAX_TRANSCRIPT_LINES) {
        transcript.erase(transcript.begin());
      }
      frame.ip += 1;
      return true;
    }
    case OP_RUN_COMMAND: {
      const Operand* cmd = requireOperand(ins, 0, OperandType::String);
      const Operand* subCount = requireOperand(ins, 1, OperandType::U16);
      if (!cmd || !subCount) return false;
      const auto substitutions = popSubstitutions(subCount->u16Value);
      transcript.push_back("[cmd] " + applySubstitutions(cmd->stringValue, substitutions));
      if (transcript.size() > MAX_TRANSCRIPT_LINES) {
        transcript.erase(transcript.begin());
      }
      frame.ip += 1;
      return true;
    }
    case OP_ADD_OPTION: {
      const Operand* lineId = requireOperand(ins, 0, OperandType::String);
      const Operand* dest = requireOperand(ins, 1, OperandType::U16);
      const Operand* subCount = requireOperand(ins, 2, OperandType::U16);
      const Operand* hasCondition = requireOperand(ins, 3, OperandType::Bool);
      if (!lineId || !dest || !subCount || !hasCondition) return false;
      bool available = true;
      if (hasCondition->boolValue) {
        if (stack.empty()) return setError("Stack underflow in add_option condition");
        available = stack.back().asBool();
        stack.pop_back();
      }
      const auto substitutions = popSubstitutions(subCount->u16Value);
      pendingOptions.push_back(
          {applySubstitutions(lookupLine(lineId->stringValue), substitutions), static_cast<int>(dest->u16Value), available});
      frame.ip += 1;
      return true;
    }
    case OP_SHOW_OPTIONS: {
      selectedOption = firstAvailableOption();
      if (selectedOption < 0) {
        return setError("No selectable options");
      }
      waitingForChoice = true;
      return true;
    }
    case OP_JUMP_TO: {
      const Operand* dest = requireOperand(ins, 0, OperandType::U16);
      if (!dest) return false;
      frame.ip = static_cast<int>(dest->u16Value);
      return true;
    }
    case OP_STOP:
      markVisited(frame.nodeName);
      frames.clear();
      finished = true;
      return true;
    case OP_RETURN:
      markVisited(frame.nodeName);
      frames.pop_back();
      return true;
    case OP_POP:
      if (stack.empty()) return setError("Stack underflow in pop");
      stack.pop_back();
      frame.ip += 1;
      return true;
    case OP_JUMP_IF_FALSE: {
      const Operand* dest = requireOperand(ins, 0, OperandType::U16);
      if (!dest) return false;
      if (stack.empty()) return setError("Stack underflow in jump_if_false");
      const bool cond = stack.back().asBool();
      frame.ip = cond ? frame.ip + 1 : static_cast<int>(dest->u16Value);
      return true;
    }
    case OP_PEEK_AND_JUMP:
      if (stack.empty()) return setError("Stack underflow in peek_and_jump");
      {
        size_t jumpValueIndex = stack.size() - 1;
        if (stack.back().type == ValueType::Bool && stack.size() >= 2) {
          // Some compiled stories leave [destination, bool] on the stack.
          jumpValueIndex = stack.size() - 2;
        }
        frame.ip = static_cast<int>(stack[jumpValueIndex].asFloat());
      }
      return true;
    case OP_RUN_NODE: {
      const Operand* nodeName = requireOperand(ins, 0, OperandType::String);
      if (!nodeName) return false;
      if (!ensureNodeLoaded(nodeName->stringValue)) {
        return false;
      }
      markVisited(frame.nodeName);
      // RUN_NODE is a jump: replace the whole call stack, do not return to detour callers.
      frames.clear();
      frames.push_back({nodeName->stringValue, 0});
      return true;
    }
    case OP_DETOUR_TO_NODE: {
      const Operand* nodeName = requireOperand(ins, 0, OperandType::String);
      if (!nodeName) return false;
      if (!ensureNodeLoaded(nodeName->stringValue)) {
        return false;
      }
      frame.ip += 1;
      frames.push_back({nodeName->stringValue, 0});
      return true;
    }
    case OP_PEEK_AND_RUN_NODE: {
      if (stack.empty()) return setError("Stack underflow in peek_and_run_node");
      const std::string target = stack.back().asString();
      stack.pop_back();
      if (!ensureNodeLoaded(target)) {
        return false;
      }
      markVisited(frame.nodeName);
      // PEEK_AND_RUN_NODE is also a jump: replace the whole call stack.
      frames.clear();
      frames.push_back({target, 0});
      return true;
    }
    case OP_PEEK_AND_DETOUR: {
      if (stack.empty()) return setError("Stack underflow in peek_and_detour");
      const std::string target = stack.back().asString();
      stack.pop_back();
      if (!ensureNodeLoaded(target)) {
        return false;
      }
      frame.ip += 1;
      frames.push_back({target, 0});
      return true;
    }
    case OP_PUSH_STRING: {
      const Operand* op = requireOperand(ins, 0, OperandType::String);
      if (!op) return false;
      stack.push_back(Value::fromString(op->stringValue));
      frame.ip += 1;
      return true;
    }
    case OP_PUSH_FLOAT: {
      const Operand* op = requireOperand(ins, 0, OperandType::Float);
      if (!op) return false;
      stack.push_back(Value::fromFloat(op->floatValue));
      frame.ip += 1;
      return true;
    }
    case OP_PUSH_BOOL: {
      const Operand* op = requireOperand(ins, 0, OperandType::Bool);
      if (!op) return false;
      stack.push_back(Value::fromBool(op->boolValue));
      frame.ip += 1;
      return true;
    }
    case OP_PUSH_VARIABLE: {
      const Operand* op = requireOperand(ins, 0, OperandType::String);
      if (!op) return false;
      auto it = state.variables.find(op->stringValue);
      if (it != state.variables.end()) {
        stack.push_back(it->second);
      } else if (nodes.find(op->stringValue) != nodes.end()) {
        Value smartValue;
        std::set<std::string> activeNodes;
        if (!evaluateSmartVariable(op->stringValue, smartValue, activeNodes)) return false;
        stack.push_back(smartValue);
      } else {
        stack.push_back(Value::fromFloat(0.0f));
      }
      frame.ip += 1;
      return true;
    }
    case OP_STORE_VARIABLE: {
      const Operand* op = requireOperand(ins, 0, OperandType::String);
      if (!op) return false;
      if (stack.empty()) return setError("Stack underflow in store_variable");
      state.variables[op->stringValue] = stack.back();  // peek, do not pop
      frame.ip += 1;
      return true;
    }
    case OP_CALL_FUNCTION: {
      const Operand* op = requireOperand(ins, 0, OperandType::String);
      if (!op) return false;
      if (stack.empty()) return setError("Stack underflow reading call arity");
      const int arity = static_cast<int>(stack.back().asFloat());
      stack.pop_back();
      if (arity < 0 || static_cast<int>(stack.size()) < arity) {
        return setError("Invalid call arity");
      }
      std::vector<Value> args;
      args.reserve(static_cast<size_t>(arity));
      for (int i = 0; i < arity; ++i) {
        args.push_back(stack.back());
        stack.pop_back();
      }
      std::reverse(args.begin(), args.end());
      stack.push_back(callFunction(op->stringValue, std::move(args)));
      frame.ip += 1;
      return true;
    }
    case OP_ADD_SALIENCY_CANDIDATE: {
      const Operand* id = requireOperand(ins, 0, OperandType::String);
      const Operand* score = requireOperand(ins, 1, OperandType::U16);
      const Operand* dest = requireOperand(ins, 2, OperandType::U16);
      if (!id || !score || !dest) return false;
      if (stack.empty()) return setError("Stack underflow in add_saliency_candidate");
      const bool conditionPassed = stack.back().asBool();
      stack.pop_back();
      if (conditionPassed) {
        saliencyCandidates.push_back({id->stringValue, score->u16Value, dest->u16Value});
      }
      frame.ip += 1;
      return true;
    }
    case OP_ADD_SALIENCY_CANDIDATE_FROM_NODE: {
      const Operand* nodeName = requireOperand(ins, 0, OperandType::String);
      const Operand* dest = requireOperand(ins, 1, OperandType::U16);
      if (!nodeName || !dest) return false;
      bool conditionPassed = true;
      const std::string conditionNode =
          "$Yarn.Internal." + frame.nodeName + "." + nodeName->stringValue + ".Condition.0";
      if (nodes.find(conditionNode) != nodes.end() || nodeIndex.find(conditionNode) != nodeIndex.end()) {
        Value conditionValue;
        std::set<std::string> activeNodes;
        if (!evaluateSmartVariable(conditionNode, conditionValue, activeNodes)) {
          return false;
        }
        conditionPassed = conditionValue.asBool();
      }
      if (conditionPassed) {
        const auto visitIt = state.visitCounts.find(nodeName->stringValue);
        const int score = (visitIt != state.visitCounts.end() && visitIt->second > 0) ? 0 : 1;
        saliencyCandidates.push_back({nodeName->stringValue, score, dest->u16Value});
      }
      frame.ip += 1;
      return true;
    }
    case OP_SELECT_SALIENCY_CANDIDATE: {
      if (saliencyCandidates.empty()) return setError("No saliency candidates");
      int bestScore = std::numeric_limits<int>::min();
      std::vector<size_t> bestScoreIndices;
      bestScoreIndices.reserve(saliencyCandidates.size());
      for (size_t i = 0; i < saliencyCandidates.size(); ++i) {
        const int score = saliencyCandidates[i].complexityScore;
        if (score > bestScore) {
          bestScore = score;
          bestScoreIndices.clear();
          bestScoreIndices.push_back(i);
        } else if (score == bestScore) {
          bestScoreIndices.push_back(i);
        }
      }

      int leastSelected = std::numeric_limits<int>::max();
      std::vector<size_t> leastRecentlyViewedIndices;
      leastRecentlyViewedIndices.reserve(bestScoreIndices.size());
      for (size_t idx : bestScoreIndices) {
        const auto countIt = state.saliencySelectionCounts.find(saliencyCandidates[idx].id);
        const int selectedCount = countIt != state.saliencySelectionCounts.end() ? countIt->second : 0;
        if (selectedCount < leastSelected) {
          leastSelected = selectedCount;
          leastRecentlyViewedIndices.clear();
          leastRecentlyViewedIndices.push_back(idx);
        } else if (selectedCount == leastSelected) {
          leastRecentlyViewedIndices.push_back(idx);
        }
      }

      std::uniform_int_distribution<size_t> dist(0, leastRecentlyViewedIndices.size() - 1);
      const size_t chosen = leastRecentlyViewedIndices[dist(state.rng)];
      state.saliencySelectionCounts[saliencyCandidates[chosen].id] += 1;

      stack.push_back(Value::fromFloat(static_cast<float>(saliencyCandidates[chosen].destination)));
      stack.push_back(Value::fromBool(true));
      saliencyCandidates.clear();
      frame.ip += 1;
      return true;
    }
    default:
      return setError("Unsupported opcode");
  }
}

bool YsaRuntime::setError(const std::string& msg) {
  errored = true;
  waitingForChoice = false;
  errorMessage = msg;
  LOG_ERR("IF", "%s", msg.c_str());
  return false;
}

void YsaRuntime::markVisited(const std::string& nodeName) {
  state.visitCounts[nodeName] += 1;
}

void YsaRuntime::resetRuntimeState() {
  state = RuntimeState{};
  state.variables = initialValues;
  frames.clear();
  stack.clear();
  pendingOptions.clear();
  saliencyCandidates.clear();
  transcript.clear();
  waitingForChoice = false;
  finished = false;
  errored = false;
  selectedOption = -1;
  errorMessage.clear();
}

bool YsaRuntime::readU8(const std::vector<uint8_t>& data, size_t& offset, uint8_t& out) const {
  if (offset + 1 > data.size()) return false;
  out = data[offset];
  offset += 1;
  return true;
}

bool YsaRuntime::readU16(const std::vector<uint8_t>& data, size_t& offset, uint16_t& out) const {
  if (offset + 2 > data.size()) return false;
  memcpy(&out, data.data() + offset, 2);
  offset += 2;
  return true;
}

bool YsaRuntime::readU32(const std::vector<uint8_t>& data, size_t& offset, uint32_t& out) const {
  if (offset + 4 > data.size()) return false;
  memcpy(&out, data.data() + offset, 4);
  offset += 4;
  return true;
}

bool YsaRuntime::readF32(const std::vector<uint8_t>& data, size_t& offset, float& out) const {
  if (offset + 4 > data.size()) return false;
  memcpy(&out, data.data() + offset, 4);
  offset += 4;
  return true;
}

bool YsaRuntime::readString(const std::vector<uint8_t>& data, size_t& offset, std::string& out) const {
  uint16_t size = 0;
  if (!readU16(data, offset, size)) return false;
  if (offset + size > data.size()) return false;
  out.assign(reinterpret_cast<const char*>(data.data() + offset), size);
  offset += size;
  return true;
}

bool YsaRuntime::buildNodeIndex(const std::vector<uint8_t>& headerData) {
  size_t offset = 0;
  if (headerData.size() < 8) {
    return setError("Story too small");
  }
  if (!(headerData[0] == 'Y' && headerData[1] == 'S' && headerData[2] == 'A' && headerData[3] == '1')) {
    return setError("Invalid YSA magic");
  }
  offset = 4;

  uint16_t nodeCount = 0;
  uint16_t initialValueCount = 0;
  uint32_t lineCount = 0;
  if (!readU16(headerData, offset, version) || !readU16(headerData, offset, nodeCount) || !readU32(headerData, offset, lineCount)) {
    return setError("Invalid header");
  }
  if (version == 2) {
    if (!readU16(headerData, offset, initialValueCount)) {
      return setError("Invalid initial value header");
    }
  } else if (version != 1) {
    return setError("Unsupported YSA version");
  }

  // Build index: store offset of first instruction for each node
  for (uint16_t i = 0; i < nodeCount; ++i) {
    std::string nodeName;
    uint16_t instructionCount = 0;
    
    if (!readString(headerData, offset, nodeName) || !readU16(headerData, offset, instructionCount)) {
      return setError("Invalid node header");
    }

    // Record where instructions START (after name + instructionCount)
    NodeIndex idx;
    idx.fileOffset = offset;  // This is the offset of the FIRST instruction
    idx.instructionCount = instructionCount;
    nodeIndex[nodeName] = idx;

    // Skip past instructions without parsing
    for (uint16_t j = 0; j < instructionCount; ++j) {
      uint16_t instructionSize = 0;
      if (!readU16(headerData, offset, instructionSize) || offset + instructionSize > headerData.size()) {
        return setError("Invalid instruction size");
      }
      offset += instructionSize;
    }
  }

  // Parse line table
  for (uint32_t i = 0; i < lineCount; ++i) {
    std::string lineId;
    std::string text;
    if (!readString(headerData, offset, lineId) || !readString(headerData, offset, text)) {
      return setError("Invalid line table");
    }
    lineTable[lineId] = text;
  }

  // Parse initial values
  for (uint16_t i = 0; i < initialValueCount; ++i) {
    std::string varName;
    uint8_t valueType = 0;
    if (!readString(headerData, offset, varName) || !readU8(headerData, offset, valueType)) {
      return setError("Invalid initial value");
    }

    Value value = Value::fromFloat(0.0f);
    switch (valueType) {
      case 0: {
        float f = 0;
        if (!readF32(headerData, offset, f)) return setError("Invalid float initial value");
        value = Value::fromFloat(f);
        break;
      }
      case 1: {
        uint8_t b = 0;
        if (!readU8(headerData, offset, b)) return setError("Invalid bool initial value");
        value = Value::fromBool(b != 0);
        break;
      }
      case 2: {
        std::string s;
        if (!readString(headerData, offset, s)) return setError("Invalid string initial value");
        value = Value::fromString(s);
        break;
      }
      default:
        return setError("Invalid initial value type");
    }
    initialValues[varName] = std::move(value);
  }

  return true;
}

const YsaRuntime::Operand* YsaRuntime::requireOperand(const Instruction& ins, size_t index, OperandType type) {
  if (index >= ins.operands.size()) {
    setError("Missing operand");
    return nullptr;
  }
  const Operand& operand = ins.operands[index];
  if (operand.type != type) {
    setError("Operand type mismatch");
    return nullptr;
  }
  return &operand;
}

std::vector<YsaRuntime::Value> YsaRuntime::popSubstitutions(int count) {
  std::vector<Value> values(static_cast<size_t>(std::max(0, count)));
  for (int i = count - 1; i >= 0; --i) {
    if (stack.empty()) {
      setError("Stack underflow in substitutions");
      break;
    }
    values[static_cast<size_t>(i)] = stack.back();
    stack.pop_back();
  }
  return values;
}

std::string YsaRuntime::lookupLine(const std::string& lineId) const {
  const auto it = lineTable.find(lineId);
  return (it != lineTable.end()) ? it->second : lineId;
}

std::string YsaRuntime::applySubstitutions(const std::string& text, const std::vector<Value>& substitutions) const {
  std::string result = text;
  for (size_t i = 0; i < substitutions.size(); ++i) {
    const std::string key = "{" + std::to_string(i) + "}";
    const std::string value = substitutions[i].asString();
    size_t pos = 0;
    while ((pos = result.find(key, pos)) != std::string::npos) {
      result.replace(pos, key.size(), value);
      pos += value.size();
    }
  }
  return result;
}

YsaRuntime::Value YsaRuntime::callFunction(std::string name, std::vector<Value> args) {
  const auto dot = name.rfind('.');
  if (dot != std::string::npos) {
    name = name.substr(dot + 1);
  }

  auto arityIt = kFunctionMinArity.find(name);
  if (arityIt == kFunctionMinArity.end()) {
    setError(std::string("Unknown function: ") + name);
    return Value::fromBool(false);
  }
  if (static_cast<int>(args.size()) < arityIt->second) {
    setError(std::string("Not enough args for function: ") + name);
    return Value::fromBool(false);
  }

  if (name == "Add") {
    if (args[0].type == ValueType::String || args[1].type == ValueType::String) {
      return Value::fromString(args[0].asString() + args[1].asString());
    }
    return Value::fromFloat(args[0].asFloat() + args[1].asFloat());
  }
  if (name == "Minus") return Value::fromFloat(args[0].asFloat() - args[1].asFloat());
  if (name == "Multiply") return Value::fromFloat(args[0].asFloat() * args[1].asFloat());
  if (name == "Divide") {
    const float rhs = args[1].asFloat();
    return Value::fromFloat(rhs == 0.0f ? 0.0f : args[0].asFloat() / rhs);
  }
  if (name == "Modulo") {
    const float rhs = args[1].asFloat();
    return Value::fromFloat(rhs == 0.0f ? 0.0f : fmodf(args[0].asFloat(), rhs));
  }
  if (name == "UnaryMinus") return Value::fromFloat(-args[0].asFloat());

  if (name == "EqualTo") {
    if (args[0].type == ValueType::String || args[1].type == ValueType::String) {
      return Value::fromBool(args[0].asString() == args[1].asString());
    }
    return Value::fromBool(args[0].asFloat() == args[1].asFloat());
  }
  if (name == "NotEqualTo") {
    if (args[0].type == ValueType::String || args[1].type == ValueType::String) {
      return Value::fromBool(args[0].asString() != args[1].asString());
    }
    return Value::fromBool(args[0].asFloat() != args[1].asFloat());
  }
  if (name == "LessThan") return Value::fromBool(args[0].asFloat() < args[1].asFloat());
  if (name == "GreaterThan") return Value::fromBool(args[0].asFloat() > args[1].asFloat());
  if (name == "LessThanOrEqualTo") return Value::fromBool(args[0].asFloat() <= args[1].asFloat());
  if (name == "GreaterThanOrEqualTo") return Value::fromBool(args[0].asFloat() >= args[1].asFloat());

  if (name == "And") return Value::fromBool(args[0].asBool() && args[1].asBool());
  if (name == "Or") return Value::fromBool(args[0].asBool() || args[1].asBool());
  if (name == "Xor") return Value::fromBool(args[0].asBool() ^ args[1].asBool());
  if (name == "Not") return Value::fromBool(!args[0].asBool());

  if (name == "visited") {
    const auto it = state.visitCounts.find(args[0].asString());
    return Value::fromBool(it != state.visitCounts.end() && it->second > 0);
  }
  if (name == "visited_count") {
    const auto it = state.visitCounts.find(args[0].asString());
    return Value::fromFloat(static_cast<float>(it != state.visitCounts.end() ? it->second : 0));
  }

  if (name == "random") {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return Value::fromFloat(dist(state.rng));
  }
  if (name == "random_range") {
    const float a = args[0].asFloat();
    const float b = args[1].asFloat();
    std::uniform_real_distribution<float> dist(std::min(a, b), std::max(a, b));
    return Value::fromFloat(dist(state.rng));
  }
  if (name == "dice") {
    const int sides = std::max(1, static_cast<int>(args[0].asFloat()));
    std::uniform_int_distribution<int> dist(1, sides);
    return Value::fromFloat(static_cast<float>(dist(state.rng)));
  }

  if (name == "round") return Value::fromFloat(roundf(args[0].asFloat()));
  if (name == "round_places") {
    const float factor = powf(10.0f, args[1].asFloat());
    return Value::fromFloat(roundf(args[0].asFloat() * factor) / factor);
  }
  if (name == "floor") return Value::fromFloat(floorf(args[0].asFloat()));
  if (name == "ceil") return Value::fromFloat(ceilf(args[0].asFloat()));
  if (name == "inc") return Value::fromFloat(args[0].asFloat() + 1.0f);
  if (name == "dec") return Value::fromFloat(args[0].asFloat() - 1.0f);
  if (name == "decimal") {
    const float v = args[0].asFloat();
    return Value::fromFloat(v - floorf(v));
  }
  if (name == "int") return Value::fromFloat(truncf(args[0].asFloat()));
  if (name == "string" || name == "format_invariant") return Value::fromString(args[0].asString());
  if (name == "number") return Value::fromFloat(args[0].asFloat());
  if (name == "bool") return Value::fromBool(args[0].asBool());
  if (name == "yn") return Value::fromString(args[0].asBool() ? "yes" : "no");

  setError(std::string("Unhandled function: ") + name);
  return Value::fromBool(false);
}

bool YsaRuntime::evaluateSmartVariable(const std::string& variableName,
                                       Value& outValue,
                                       std::set<std::string>& activeNodes) {
  if (activeNodes.count(variableName) > 0) {
    return setError(std::string("Recursive smart variable: ") + variableName);
  }

  if (!ensureNodeLoaded(variableName, &activeNodes)) {
    return false;
  }

  activeNodes.insert(variableName);
  touchNode(variableName);
  std::vector<Value> evalStack;
  int ip = 0;
  const Node& node = nodes[variableName];

  while (ip >= 0 && ip < static_cast<int>(node.instructions.size())) {
    const Instruction& ins = node.instructions[static_cast<size_t>(ip)];
    switch (ins.opcode) {
      case OP_PUSH_STRING: {
        const Operand* op = requireOperand(ins, 0, OperandType::String);
        if (!op) {
          activeNodes.erase(variableName);
          return false;
        }
        evalStack.push_back(Value::fromString(op->stringValue));
        ip += 1;
        break;
      }
      case OP_PUSH_FLOAT: {
        const Operand* op = requireOperand(ins, 0, OperandType::Float);
        if (!op) {
          activeNodes.erase(variableName);
          return false;
        }
        evalStack.push_back(Value::fromFloat(op->floatValue));
        ip += 1;
        break;
      }
      case OP_PUSH_BOOL: {
        const Operand* op = requireOperand(ins, 0, OperandType::Bool);
        if (!op) {
          activeNodes.erase(variableName);
          return false;
        }
        evalStack.push_back(Value::fromBool(op->boolValue));
        ip += 1;
        break;
      }
      case OP_PUSH_VARIABLE: {
        const Operand* op = requireOperand(ins, 0, OperandType::String);
        if (!op) {
          activeNodes.erase(variableName);
          return false;
        }
        auto it = state.variables.find(op->stringValue);
        if (it != state.variables.end()) {
          evalStack.push_back(it->second);
        } else if (nodeIndex.find(op->stringValue) != nodeIndex.end()) {
          Value nestedValue;
          if (!evaluateSmartVariable(op->stringValue, nestedValue, activeNodes)) {
            activeNodes.erase(variableName);
            return false;
          }
          evalStack.push_back(nestedValue);
        } else {
          evalStack.push_back(Value::fromFloat(0.0f));
        }
        ip += 1;
        break;
      }
      case OP_STORE_VARIABLE: {
        const Operand* op = requireOperand(ins, 0, OperandType::String);
        if (!op) {
          activeNodes.erase(variableName);
          return false;
        }
        if (evalStack.empty()) {
          activeNodes.erase(variableName);
          return setError("Stack underflow in smart variable store");
        }
        state.variables[op->stringValue] = evalStack.back();
        ip += 1;
        break;
      }
      case OP_CALL_FUNCTION: {
        const Operand* op = requireOperand(ins, 0, OperandType::String);
        if (!op) {
          activeNodes.erase(variableName);
          return false;
        }
        if (evalStack.empty()) {
          activeNodes.erase(variableName);
          return setError("Stack underflow reading smart variable call arity");
        }
        const int arity = static_cast<int>(evalStack.back().asFloat());
        evalStack.pop_back();
        if (arity < 0 || static_cast<int>(evalStack.size()) < arity) {
          activeNodes.erase(variableName);
          return setError("Invalid smart variable call arity");
        }
        std::vector<Value> args;
        args.reserve(static_cast<size_t>(arity));
        for (int i = 0; i < arity; ++i) {
          args.push_back(evalStack.back());
          evalStack.pop_back();
        }
        std::reverse(args.begin(), args.end());
        evalStack.push_back(callFunction(op->stringValue, std::move(args)));
        if (errored) {
          activeNodes.erase(variableName);
          return false;
        }
        ip += 1;
        break;
      }
      case OP_POP:
        if (evalStack.empty()) {
          activeNodes.erase(variableName);
          return setError("Stack underflow in smart variable pop");
        }
        evalStack.pop_back();
        ip += 1;
        break;
      case OP_JUMP_TO: {
        const Operand* dest = requireOperand(ins, 0, OperandType::U16);
        if (!dest) {
          activeNodes.erase(variableName);
          return false;
        }
        ip = static_cast<int>(dest->u16Value);
        break;
      }
      case OP_JUMP_IF_FALSE: {
        const Operand* dest = requireOperand(ins, 0, OperandType::U16);
        if (!dest) {
          activeNodes.erase(variableName);
          return false;
        }
        if (evalStack.empty()) {
          activeNodes.erase(variableName);
          return setError("Stack underflow in smart variable jump_if_false");
        }
        const bool cond = evalStack.back().asBool();
        ip = cond ? ip + 1 : static_cast<int>(dest->u16Value);
        break;
      }
      case OP_RETURN:
      case OP_STOP:
        ip = static_cast<int>(node.instructions.size());
        break;
      default:
        activeNodes.erase(variableName);
        return setError(std::string("Unsupported opcode in smart variable: ") + std::to_string(ins.opcode));
    }
  }

  if (evalStack.empty()) {
    activeNodes.erase(variableName);
    return setError(std::string("Smart variable produced no value: ") + variableName);
  }
  outValue = evalStack.back();
  activeNodes.erase(variableName);
  return true;
}

int YsaRuntime::firstAvailableOption() const {
  for (size_t i = 0; i < pendingOptions.size(); ++i) {
    if (pendingOptions[i].available) {
      return static_cast<int>(i);
    }
  }
  return -1;
}
