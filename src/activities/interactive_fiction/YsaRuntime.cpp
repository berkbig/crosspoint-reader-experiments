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

  // Read file in 4KB chunks to build index WITHOUT loading entire file
  constexpr size_t CHUNK_SIZE = 4096;
  uint8_t chunkBuffer[CHUNK_SIZE];
  std::vector<uint8_t> indexData;  // Small buffer for header only
  indexData.reserve(std::min(fileSize, CHUNK_SIZE * 2));
  
  size_t bytesRead = 0;
  while (bytesRead < fileSize && indexData.size() < indexData.capacity()) {
    const size_t toRead = std::min(CHUNK_SIZE, fileSize - bytesRead);
    const int result = file.read(chunkBuffer, toRead);
    if (result <= 0) {
      return setError("Failed to read story file");
    }
    indexData.insert(indexData.end(), chunkBuffer, chunkBuffer + result);
    bytesRead += result;
  }

  if (!buildNodeIndex(indexData)) {
    return false;
  }

  errored = false;
  errorMessage.clear();
  return true;
}

bool YsaRuntime::ensureNodeLoaded(const std::string& nodeName) {
  // If already in cache, we're done
  if (nodes.find(nodeName) != nodes.end()) {
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

  // We need to read from the file offset where instructions start
  // But we don't have seek() on HalFile, so we read from start and skip to the offset
  // This is inefficient but necessary given the HAL API
  
  std::vector<uint8_t> buffer;
  buffer.reserve(std::min(size_t(100000), idx.fileOffset + 10000));
  
  uint8_t readBuf[4096];
  size_t totalRead = 0;
  size_t targetSize = idx.fileOffset + 10000;  // Read enough to cover the node
  
  while (totalRead < targetSize && buffer.size() < targetSize) {
   int n = file.read(readBuf, sizeof(readBuf));
   if (n <= 0) break;
   buffer.insert(buffer.end(), readBuf, readBuf + n);
   totalRead += n;
  }

  if (buffer.size() < idx.fileOffset + 2) {
   return setError("File too small to contain node");
  }

  // Parse just this node's instructions
  size_t offset = idx.fileOffset;
  Node node;
  node.instructions.reserve(idx.instructionCount);

  for (uint16_t j = 0; j < idx.instructionCount; ++j) {
   uint16_t instructionSize = 0;
   if (!readU16(buffer, offset, instructionSize) || offset + instructionSize > buffer.size()) {
     return setError("Invalid instruction size during lazy load");
   }
   const size_t instructionEnd = offset + instructionSize;

   Instruction ins;
   uint8_t operandCount = 0;
   if (!readU8(buffer, offset, ins.opcode) || !readU8(buffer, offset, operandCount)) {
     return setError("Invalid instruction header during lazy load");
   }

   for (uint8_t k = 0; k < operandCount; ++k) {
     uint8_t typeRaw = 0;
     if (!readU8(buffer, offset, typeRaw)) {
       return setError("Invalid operand type during lazy load");
     }
     Operand operand;
     operand.type = static_cast<OperandType>(typeRaw);
     switch (operand.type) {
       case OperandType::U16:
         if (!readU16(buffer, offset, operand.u16Value)) return setError("Invalid u16 operand during lazy load");
         break;
       case OperandType::Bool: {
         uint8_t value = 0;
         if (!readU8(buffer, offset, value)) return setError("Invalid bool operand during lazy load");
         operand.boolValue = value != 0;
         break;
       }
       case OperandType::String:
         if (!readString(buffer, offset, operand.stringValue)) return setError("Invalid string operand during lazy load");
         break;
       case OperandType::Float:
         if (!readF32(buffer, offset, operand.floatValue)) return setError("Invalid float operand during lazy load");
         break;
       default:
         return setError("Unknown operand type during lazy load");
     }
     ins.operands.push_back(std::move(operand));
   }

   if (offset != instructionEnd) {
     return setError("Instruction length mismatch during lazy load");
   }
   node.instructions.push_back(std::move(ins));
  }

  // Cache the parsed node
  nodes[nodeName] = std::move(node);
  return true;
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
      const bool cond = stack.back().asBool();  // peek
      frame.ip = cond ? frame.ip + 1 : static_cast<int>(dest->u16Value);
      return true;
    }
    case OP_PEEK_AND_JUMP:
      if (stack.empty()) return setError("Stack underflow in peek_and_jump");
      frame.ip = static_cast<int>(stack.back().asFloat());
      return true;
    case OP_RUN_NODE: {
      const Operand* nodeName = requireOperand(ins, 0, OperandType::String);
      if (!nodeName) return false;
      if (!ensureNodeLoaded(nodeName->stringValue)) {
        return false;
      }
      markVisited(frame.nodeName);
      frame.nodeName = nodeName->stringValue;
      frame.ip = 0;
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
      frame.nodeName = target;
      frame.ip = 0;
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
      saliencyCandidates.push_back({id->stringValue, score->u16Value, dest->u16Value});
      frame.ip += 1;
      return true;
    }
    case OP_ADD_SALIENCY_CANDIDATE_FROM_NODE: {
      const Operand* nodeName = requireOperand(ins, 0, OperandType::String);
      const Operand* dest = requireOperand(ins, 1, OperandType::U16);
      if (!nodeName || !dest) return false;
      const int score = state.visitedNodes.count(nodeName->stringValue) > 0 ? 0 : 1;
      saliencyCandidates.push_back({nodeName->stringValue, score, dest->u16Value});
      frame.ip += 1;
      return true;
    }
    case OP_SELECT_SALIENCY_CANDIDATE: {
      if (saliencyCandidates.empty()) return setError("No saliency candidates");
      const auto best =
          std::max_element(saliencyCandidates.begin(), saliencyCandidates.end(),
                           [](const SaliencyCandidate& a, const SaliencyCandidate& b) { return a.complexityScore < b.complexityScore; });
      stack.push_back(Value::fromFloat(static_cast<float>(best->destination)));
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
  state.visitedNodes.insert(nodeName);
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

  if (name == "visited") return Value::fromBool(state.visitedNodes.count(args[0].asString()) > 0);
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

  if (!ensureNodeLoaded(variableName)) {
    return false;
  }

  activeNodes.insert(variableName);
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
