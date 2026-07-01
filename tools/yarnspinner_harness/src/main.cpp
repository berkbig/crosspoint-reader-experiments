#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "yarn_spinner.pb.h"

// ── Runtime value ──────────────────────────────────────────────────────────────

struct Value {
    enum class Type { Float, Bool, String } type = Type::Float;
    float floatValue = 0.0f;
    bool boolValue = false;
    std::string stringValue;

    static Value fromFloat(float f) { Value v; v.type = Type::Float; v.floatValue = f; return v; }
    static Value fromBool(bool b)   { Value v; v.type = Type::Bool;  v.boolValue = b;  return v; }
    static Value fromString(std::string s) { Value v; v.type = Type::String; v.stringValue = std::move(s); return v; }

    bool asBool() const {
        switch (type) {
            case Type::Bool:   return boolValue;
            case Type::Float:  return floatValue != 0.0f;
            case Type::String: return !stringValue.empty();
        }
        return false;
    }

    float asFloat() const {
        switch (type) {
            case Type::Float: return floatValue;
            case Type::Bool:  return boolValue ? 1.0f : 0.0f;
            case Type::String: try { return std::stof(stringValue); } catch (...) { return 0.0f; }
        }
        return 0.0f;
    }

    std::string asString() const {
        switch (type) {
            case Type::String: return stringValue;
            case Type::Bool:   return boolValue ? "true" : "false";
            case Type::Float: {
                float f = floatValue;
                if (f == std::floor(f) && f >= -1e9f && f <= 1e9f) {
                    std::ostringstream oss; oss << static_cast<long long>(f); return oss.str();
                }
                std::ostringstream oss; oss << f; return oss.str();
            }
        }
        return "";
    }
};

// ── YSA structures ─────────────────────────────────────────────────────────────

enum class YsaOperandType : uint8_t { U16 = 0, Bool = 1, String = 2, Float = 3 };

struct YsaOperand {
    YsaOperandType type{};
    uint16_t u16Value = 0;
    bool boolValue = false;
    std::string stringValue;
    float floatValue = 0.0f;
};

struct YsaInstruction {
    uint8_t opcode = 0;
    std::vector<YsaOperand> operands;
};

struct YsaNode {
    std::string name;
    std::vector<YsaInstruction> instructions;
};

struct YsaProgram {
    uint16_t version = 0;
    std::map<std::string, YsaNode> nodes;
    std::map<std::string, std::string> lineTable;
    std::map<std::string, Value> initialValues;
};

// ── Opcodes ────────────────────────────────────────────────────────────────────

constexpr uint8_t OP_RUN_LINE                         = 1;
constexpr uint8_t OP_RUN_COMMAND                      = 2;
constexpr uint8_t OP_ADD_OPTION                       = 3;
constexpr uint8_t OP_SHOW_OPTIONS                     = 4;
constexpr uint8_t OP_JUMP_TO                          = 5;
constexpr uint8_t OP_STOP                             = 6;
constexpr uint8_t OP_RUN_NODE                         = 7;
constexpr uint8_t OP_JUMP_IF_FALSE                    = 8;
constexpr uint8_t OP_PEEK_AND_JUMP                    = 9;
constexpr uint8_t OP_RETURN                           = 10;
constexpr uint8_t OP_POP                              = 11;
constexpr uint8_t OP_PUSH_STRING                      = 12;
constexpr uint8_t OP_PUSH_FLOAT                       = 13;
constexpr uint8_t OP_PUSH_BOOL                        = 14;
constexpr uint8_t OP_PUSH_VARIABLE                    = 15;
constexpr uint8_t OP_STORE_VARIABLE                   = 16;
constexpr uint8_t OP_CALL_FUNCTION                    = 17;
constexpr uint8_t OP_DETOUR_TO_NODE                   = 18;
constexpr uint8_t OP_PEEK_AND_DETOUR                  = 19;
constexpr uint8_t OP_PEEK_AND_RUN_NODE                = 20;
constexpr uint8_t OP_ADD_SALIENCY_CANDIDATE           = 21;
constexpr uint8_t OP_ADD_SALIENCY_CANDIDATE_FROM_NODE = 22;
constexpr uint8_t OP_SELECT_SALIENCY_CANDIDATE        = 23;

// ── Runtime state ──────────────────────────────────────────────────────────────

struct RuntimeState {
    std::map<std::string, Value> variables;
    std::set<std::string> visitedNodes;
    std::map<std::string, int> visitCounts;
    std::mt19937 rng{std::random_device{}()};
};

// ── Binary I/O helpers ─────────────────────────────────────────────────────────

bool ReadExact(const std::vector<uint8_t>& buf, size_t& off, void* dst, size_t n) {
    if (off + n > buf.size()) return false;
    std::memcpy(dst, buf.data() + off, n);
    off += n;
    return true;
}
bool ReadU8(const std::vector<uint8_t>& buf, size_t& off, uint8_t& v)   { return ReadExact(buf, off, &v, 1); }
bool ReadU16(const std::vector<uint8_t>& buf, size_t& off, uint16_t& v) { return ReadExact(buf, off, &v, 2); }
bool ReadU32(const std::vector<uint8_t>& buf, size_t& off, uint32_t& v) { return ReadExact(buf, off, &v, 4); }
bool ReadF32(const std::vector<uint8_t>& buf, size_t& off, float& v)    { return ReadExact(buf, off, &v, 4); }

bool ReadString(const std::vector<uint8_t>& buf, size_t& off, std::string& v) {
    uint16_t len = 0;
    if (!ReadU16(buf, off, len) || off + len > buf.size()) return false;
    v.assign(reinterpret_cast<const char*>(buf.data() + off), len);
    off += len;
    return true;
}

// ── .yarnc loaders ─────────────────────────────────────────────────────────────

bool LoadProgramFromFile(const std::filesystem::path& path, Yarn::Program& program) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open program file: " << path << "\n";
        return false;
    }
    if (!program.ParseFromIstream(&file)) {
        std::cerr << "Failed to parse program from: " << path << "\n";
        return false;
    }
    std::cout << "Parsed .yarnc: " << program.nodes_size() << " nodes\n";
    return true;
}

std::map<std::string, std::string> LoadLineTable(const std::filesystem::path& path) {
    std::map<std::string, std::string> lines;
    std::ifstream file(path);
    if (!file.is_open()) return lines;

    auto parseCsvRow = [](const std::string& row) -> std::vector<std::string> {
        std::vector<std::string> cols;
        std::string cur;
        bool inQuotes = false;
        for (size_t i = 0; i < row.size(); ++i) {
            const char c = row[i];
            if (c == '"') {
                if (inQuotes && i + 1 < row.size() && row[i + 1] == '"') {
                    cur.push_back('"');
                    ++i;
                } else {
                    inQuotes = !inQuotes;
                }
            } else if (c == ',' && !inQuotes) {
                cols.push_back(cur);
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
        cols.push_back(cur);
        return cols;
    };

    std::string header;
    if (!std::getline(file, header)) return lines;
    const auto headerCols = parseCsvRow(header);
    int idCol = -1;
    int textCol = -1;
    for (size_t i = 0; i < headerCols.size(); ++i) {
        if (headerCols[i] == "id") idCol = static_cast<int>(i);
        if (headerCols[i] == "text") textCol = static_cast<int>(i);
    }
    if (idCol < 0 || textCol < 0) return lines;

    std::string row;
    while (std::getline(file, row)) {
        if (row.empty()) continue;
        const auto cols = parseCsvRow(row);
        if (static_cast<int>(cols.size()) <= std::max(idCol, textCol)) continue;
        const std::string& id = cols[static_cast<size_t>(idCol)];
        const std::string& text = cols[static_cast<size_t>(textCol)];
        if (!id.empty() && !text.empty()) lines[id] = text;
    }
    return lines;
}

// ── YSA loader ─────────────────────────────────────────────────────────────────

bool LoadYsaProgramFromFile(const std::filesystem::path& path, YsaProgram& program) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) { std::cerr << "Failed to open YSA file: " << path << "\n"; return false; }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    size_t off = 0;
    uint8_t magic[4]{};
    if (!ReadExact(data, off, magic, 4) ||
        !(magic[0]=='Y' && magic[1]=='S' && magic[2]=='A' && magic[3]=='1')) {
        std::cerr << "Invalid YSA magic\n"; return false;
    }

    uint16_t version = 0, nodeCount = 0, initialValueCount = 0;
    uint32_t lineCount = 0;
    if (!ReadU16(data, off, version) || !ReadU16(data, off, nodeCount) || !ReadU32(data, off, lineCount)) {
        std::cerr << "Failed to read YSA header\n"; return false;
    }
    if (version == 2) {
        if (!ReadU16(data, off, initialValueCount)) { std::cerr << "Failed to read initial_value_count\n"; return false; }
    } else if (version != 1) {
        std::cerr << "Unsupported YSA version: " << version << "\n"; return false;
    }

    program.version = version;
    program.nodes.clear(); program.lineTable.clear(); program.initialValues.clear();

    for (uint16_t ni = 0; ni < nodeCount; ++ni) {
        YsaNode node;
        if (!ReadString(data, off, node.name)) { std::cerr << "Failed to read node name\n"; return false; }
        uint16_t instrCount = 0;
        if (!ReadU16(data, off, instrCount)) { std::cerr << "Failed to read instruction count\n"; return false; }
        node.instructions.reserve(instrCount);
        for (uint16_t ii = 0; ii < instrCount; ++ii) {
            uint16_t instrSize = 0;
            if (!ReadU16(data, off, instrSize) || off + instrSize > data.size()) {
                std::cerr << "Bad instruction in node " << node.name << "\n"; return false;
            }
            const size_t instrEnd = off + instrSize;
            YsaInstruction instr;
            uint8_t opCount = 0;
            if (!ReadU8(data, off, instr.opcode) || !ReadU8(data, off, opCount)) {
                std::cerr << "Failed to read instruction header\n"; return false;
            }
            instr.operands.reserve(opCount);
            for (uint8_t oi = 0; oi < opCount; ++oi) {
                uint8_t typeRaw = 0;
                if (!ReadU8(data, off, typeRaw)) { std::cerr << "Failed to read operand type\n"; return false; }
                YsaOperand op; op.type = static_cast<YsaOperandType>(typeRaw);
                switch (op.type) {
                    case YsaOperandType::U16:    if (!ReadU16(data, off, op.u16Value))   { std::cerr << "Bad u16 operand\n"; return false; } break;
                    case YsaOperandType::Bool:   { uint8_t b=0; if (!ReadU8(data, off, b)) { std::cerr << "Bad bool operand\n"; return false; } op.boolValue = b!=0; break; }
                    case YsaOperandType::String: if (!ReadString(data, off, op.stringValue)) { std::cerr << "Bad string operand\n"; return false; } break;
                    case YsaOperandType::Float:  if (!ReadF32(data, off, op.floatValue)) { std::cerr << "Bad float operand\n"; return false; } break;
                    default: std::cerr << "Unknown operand type " << (int)typeRaw << "\n"; return false;
                }
                instr.operands.push_back(std::move(op));
            }
            if (off != instrEnd) { std::cerr << "Instruction size mismatch in node " << node.name << "\n"; return false; }
            node.instructions.push_back(std::move(instr));
        }
        program.nodes[node.name] = std::move(node);
    }

    for (uint32_t li = 0; li < lineCount; ++li) {
        std::string id, text;
        if (!ReadString(data, off, id) || !ReadString(data, off, text)) { std::cerr << "Failed to read line table\n"; return false; }
        program.lineTable[id] = text;
    }

    for (uint16_t vi = 0; vi < initialValueCount; ++vi) {
        std::string varName; uint8_t valType = 0;
        if (!ReadString(data, off, varName) || !ReadU8(data, off, valType)) { std::cerr << "Failed to read initial value\n"; return false; }
        Value val;
        switch (valType) {
            case 0: { float f=0; if (!ReadF32(data, off, f)) return false; val = Value::fromFloat(f); break; }
            case 1: { uint8_t b=0; if (!ReadU8(data, off, b)) return false; val = Value::fromBool(b!=0); break; }
            case 2: { std::string s; if (!ReadString(data, off, s)) return false; val = Value::fromString(s); break; }
            default: std::cerr << "Unknown initial value type " << (int)valType << "\n"; return false;
        }
        program.initialValues[varName] = std::move(val);
    }

    if (off != data.size()) { std::cerr << "Trailing bytes in YSA (" << (data.size()-off) << " extra)\n"; return false; }
    std::cout << "Parsed YSA v" << version << ": " << program.nodes.size() << " nodes, "
              << program.lineTable.size() << " lines, " << program.initialValues.size() << " initial values\n";
    return true;
}

// ── Text utilities ─────────────────────────────────────────────────────────────

std::string LookupLine(const std::string& lineId, const std::map<std::string, std::string>& lineTable) {
    auto it = lineTable.find(lineId);
    return it != lineTable.end() ? it->second : lineId;
}

std::string ApplySubstitutions(const std::string& text, const std::vector<Value>& subs) {
    if (subs.empty()) return text;
    std::string result = text;
    for (size_t i = 0; i < subs.size(); ++i) {
        const std::string placeholder = "{" + std::to_string(i) + "}";
        const std::string replacement = subs[i].asString();
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.size(), replacement);
            pos += replacement.size();
        }
    }
    return result;
}

// ── Built-in function library ──────────────────────────────────────────────────

// Arithmetic operators are emitted by ysc as call_function with these names.
static const std::map<std::string, int> kFunctionArity = {
    {"Add",2},{"Minus",2},{"Multiply",2},{"Divide",2},{"Modulo",2},
    {"EqualTo",2},{"NotEqualTo",2},
    {"LessThan",2},{"GreaterThan",2},{"LessThanOrEqualTo",2},{"GreaterThanOrEqualTo",2},
    {"And",2},{"Or",2},{"Xor",2},{"Not",1},{"UnaryMinus",1},
    {"visited",1},{"visited_count",1},
    {"random",0},{"random_range",2},{"dice",1},
    {"round",1},{"round_places",2},{"floor",1},{"ceil",1},
    {"inc",1},{"dec",1},{"decimal",1},{"int",1},
    {"string",1},{"number",1},{"bool",1},{"yn",1},{"format_invariant",1},
};

Value CallFunction(const std::string& name, std::vector<Value> args, RuntimeState& state) {
    // Arithmetic
    if (name == "Add") {
        if (args[0].type == Value::Type::String || args[1].type == Value::Type::String)
            return Value::fromString(args[0].asString() + args[1].asString());
        return Value::fromFloat(args[0].asFloat() + args[1].asFloat());
    }
    if (name == "Minus")    return Value::fromFloat(args[0].asFloat() - args[1].asFloat());
    if (name == "Multiply") return Value::fromFloat(args[0].asFloat() * args[1].asFloat());
    if (name == "Divide") {
        float b = args[1].asFloat();
        if (b == 0.0f) { std::cerr << "Division by zero\n"; return Value::fromFloat(0.0f); }
        return Value::fromFloat(args[0].asFloat() / b);
    }
    if (name == "Modulo") {
        float b = args[1].asFloat();
        if (b == 0.0f) { std::cerr << "Modulo by zero\n"; return Value::fromFloat(0.0f); }
        return Value::fromFloat(std::fmod(args[0].asFloat(), b));
    }
    if (name == "UnaryMinus") return Value::fromFloat(-args[0].asFloat());

    // Comparison
    if (name == "EqualTo") {
        if (args[0].type == Value::Type::String || args[1].type == Value::Type::String)
            return Value::fromBool(args[0].asString() == args[1].asString());
        if (args[0].type == Value::Type::Bool && args[1].type == Value::Type::Bool)
            return Value::fromBool(args[0].boolValue == args[1].boolValue);
        return Value::fromBool(args[0].asFloat() == args[1].asFloat());
    }
    if (name == "NotEqualTo") {
        if (args[0].type == Value::Type::String || args[1].type == Value::Type::String)
            return Value::fromBool(args[0].asString() != args[1].asString());
        if (args[0].type == Value::Type::Bool && args[1].type == Value::Type::Bool)
            return Value::fromBool(args[0].boolValue != args[1].boolValue);
        return Value::fromBool(args[0].asFloat() != args[1].asFloat());
    }
    if (name == "LessThan")             return Value::fromBool(args[0].asFloat() <  args[1].asFloat());
    if (name == "GreaterThan")          return Value::fromBool(args[0].asFloat() >  args[1].asFloat());
    if (name == "LessThanOrEqualTo")    return Value::fromBool(args[0].asFloat() <= args[1].asFloat());
    if (name == "GreaterThanOrEqualTo") return Value::fromBool(args[0].asFloat() >= args[1].asFloat());

    // Boolean
    if (name == "And") return Value::fromBool(args[0].asBool() && args[1].asBool());
    if (name == "Or")  return Value::fromBool(args[0].asBool() || args[1].asBool());
    if (name == "Xor") return Value::fromBool(args[0].asBool() ^  args[1].asBool());
    if (name == "Not") return Value::fromBool(!args[0].asBool());

    // Yarn built-ins
    if (name == "visited")
        return Value::fromBool(state.visitedNodes.count(args[0].asString()) > 0);
    if (name == "visited_count") {
        auto it = state.visitCounts.find(args[0].asString());
        return Value::fromFloat(static_cast<float>(it != state.visitCounts.end() ? it->second : 0));
    }
    if (name == "random") {
        std::uniform_real_distribution<float> d(0.0f, 1.0f);
        return Value::fromFloat(d(state.rng));
    }
    if (name == "random_range") {
        float a = args[0].asFloat(), b = args[1].asFloat();
        std::uniform_real_distribution<float> d(std::min(a,b), std::max(a,b));
        return Value::fromFloat(d(state.rng));
    }
    if (name == "dice") {
        int sides = std::max(1, static_cast<int>(args[0].asFloat()));
        std::uniform_int_distribution<int> d(1, sides);
        return Value::fromFloat(static_cast<float>(d(state.rng)));
    }
    if (name == "round")        return Value::fromFloat(std::round(args[0].asFloat()));
    if (name == "round_places") {
        float factor = std::pow(10.0f, args[1].asFloat());
        return Value::fromFloat(std::round(args[0].asFloat() * factor) / factor);
    }
    if (name == "floor")   return Value::fromFloat(std::floor(args[0].asFloat()));
    if (name == "ceil")    return Value::fromFloat(std::ceil(args[0].asFloat()));
    if (name == "inc")     return Value::fromFloat(args[0].asFloat() + 1.0f);
    if (name == "dec")     return Value::fromFloat(args[0].asFloat() - 1.0f);
    if (name == "decimal") { float n = args[0].asFloat(); return Value::fromFloat(n - std::floor(n)); }
    if (name == "int")     return Value::fromFloat(std::trunc(args[0].asFloat()));
    if (name == "string" || name == "format_invariant") return Value::fromString(args[0].asString());
    if (name == "number")  return Value::fromFloat(args[0].asFloat());
    if (name == "bool")    return Value::fromBool(args[0].asBool());
    if (name == "yn")      return Value::fromString(args[0].asBool() ? "yes" : "no");

    std::cerr << "Unknown function: " << name << "\n";
    return Value::fromBool(false);
}

// ── Option choice ──────────────────────────────────────────────────────────────

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

// ── UI helpers ─────────────────────────────────────────────────────────────────

int PromptOptionSelection(const std::vector<OptionChoice>& options) {
    std::vector<size_t> available;
    for (size_t i = 0; i < options.size(); ++i)
        if (options[i].available) available.push_back(i);
    if (available.empty()) return -1;

    for (size_t i = 0; i < available.size(); ++i)
        std::cout << "  " << (i+1) << ". " << options[available[i]].resolvedText << "\n";
    std::cout << "Select option [1-" << available.size() << "]: ";

    while (true) {
        std::string input;
        if (!std::getline(std::cin, input)) return static_cast<int>(available[0]);
        std::istringstream ss(input);
        int choice = 0;
        if (ss >> choice && choice >= 1 && choice <= static_cast<int>(available.size()))
            return static_cast<int>(available[static_cast<size_t>(choice - 1)]);
        std::cout << "Invalid. Select [1-" << available.size() << "]: ";
    }
}

int AutoSelectOption(const std::vector<OptionChoice>& options) {
    for (size_t i = 0; i < options.size(); ++i)
        if (options[i].available) return static_cast<int>(i);
    return -1;
}

// ── Helper: pop substitution values from stack ─────────────────────────────────

std::vector<Value> PopSubstitutions(std::vector<Value>& stack, int count) {
    std::vector<Value> subs(static_cast<size_t>(count));
    for (int i = count - 1; i >= 0; --i) {
        if (stack.empty()) { std::cerr << "Stack underflow in substitution pop\n"; break; }
        subs[static_cast<size_t>(i)] = stack.back();
        stack.pop_back();
    }
    return subs;
}

// ── YSA executor ──────────────────────────────────────────────────────────────

bool ExecuteNodeYsa(const YsaProgram& program, const std::string& nodeName, bool interactive, RuntimeState& state);

bool ExecuteNodeYsa(const YsaProgram& program, const std::string& nodeName, bool interactive, RuntimeState& state) {
    auto it = program.nodes.find(nodeName);
    if (it == program.nodes.end()) { std::cerr << "Node not found: " << nodeName << "\n"; return false; }

    const YsaNode& node = it->second;
    std::cout << "\n--- " << nodeName << " ---\n";

    std::vector<Value> stack;
    std::vector<OptionChoice> pendingOptions;
    std::vector<SaliencyCandidate> saliencyCandidates;
    int ip = 0;

    // Mark visited on all success exits (not before, so visited() returns false on the first visit)
    auto markVisited = [&]() {
        state.visitedNodes.insert(nodeName);
        state.visitCounts[nodeName]++;
    };

    auto requireOp = [&](const YsaInstruction& ins, size_t idx, YsaOperandType t) -> const YsaOperand* {
        if (idx >= ins.operands.size() || ins.operands[idx].type != t) {
            std::cerr << "Operand error at opcode " << (int)ins.opcode << " in " << nodeName << "\n";
            return nullptr;
        }
        return &ins.operands[idx];
    };

    while (ip >= 0 && ip < static_cast<int>(node.instructions.size())) {
        const YsaInstruction& ins = node.instructions[static_cast<size_t>(ip)];

        switch (ins.opcode) {

            case OP_RUN_LINE: {
                const auto* lid = requireOp(ins, 0, YsaOperandType::String);
                const auto* sc  = requireOp(ins, 1, YsaOperandType::U16);
                if (!lid || !sc) return false;
                auto subs = PopSubstitutions(stack, sc->u16Value);
                std::cout << ApplySubstitutions(LookupLine(lid->stringValue, program.lineTable), subs) << "\n";
                ++ip; break;
            }

            case OP_RUN_COMMAND: {
                const auto* cmd = requireOp(ins, 0, YsaOperandType::String);
                const auto* sc  = requireOp(ins, 1, YsaOperandType::U16);
                if (!cmd || !sc) return false;
                auto subs = PopSubstitutions(stack, sc->u16Value);
                std::cout << "[command] " << ApplySubstitutions(cmd->stringValue, subs) << "\n";
                ++ip; break;
            }

            case OP_ADD_OPTION: {
                const auto* lid  = requireOp(ins, 0, YsaOperandType::String);
                const auto* dest = requireOp(ins, 1, YsaOperandType::U16);
                const auto* sc   = requireOp(ins, 2, YsaOperandType::U16);
                const auto* hc   = requireOp(ins, 3, YsaOperandType::Bool);
                if (!lid || !dest || !sc || !hc) return false;
                bool available = true;
                if (hc->boolValue) {
                    if (stack.empty()) { std::cerr << "Stack empty for conditional option\n"; return false; }
                    available = stack.back().asBool();
                    stack.pop_back();
                }
                auto subs = PopSubstitutions(stack, sc->u16Value);
                pendingOptions.push_back({ApplySubstitutions(LookupLine(lid->stringValue, program.lineTable), subs), dest->u16Value, available});
                ++ip; break;
            }

            case OP_SHOW_OPTIONS: {
                if (pendingOptions.empty()) { std::cerr << "No options to show\n"; return false; }
                std::cout << "Options:\n";
                const int sel = interactive ? PromptOptionSelection(pendingOptions) : AutoSelectOption(pendingOptions);
                if (sel < 0) { std::cerr << "No available options\n"; return false; }
                if (!interactive) std::cout << "  Auto-selecting: " << pendingOptions[static_cast<size_t>(sel)].resolvedText << "\n";
                const int dest = pendingOptions[static_cast<size_t>(sel)].destination;
                pendingOptions.clear();
                stack.push_back(Value::fromFloat(static_cast<float>(dest)));
                stack.push_back(Value::fromBool(true));
                ++ip; break;
            }

            case OP_JUMP_TO: {
                const auto* dest = requireOp(ins, 0, YsaOperandType::U16);
                if (!dest) return false;
                ip = dest->u16Value; break;
            }

            case OP_JUMP_IF_FALSE: {
                const auto* dest = requireOp(ins, 0, YsaOperandType::U16);
                if (!dest) return false;
                if (stack.empty()) { std::cerr << "Stack empty for jump_if_false\n"; return false; }
                const bool cond = stack.back().asBool(); // PEEK — value stays; pop at jump target cleans it
                ip = cond ? ip + 1 : dest->u16Value; break;
            }

            case OP_PEEK_AND_JUMP: {
                if (stack.empty()) { std::cerr << "Stack empty for peek_and_jump\n"; return false; }
                ip = static_cast<int>(stack.back().asFloat());
                // Value stays on stack; option block will pop it
                break;
            }

            case OP_POP:
                if (stack.empty()) { std::cerr << "Stack empty for pop\n"; return false; }
                stack.pop_back(); ++ip; break;

            case OP_STOP:   std::cout << "[stop]\n"; markVisited(); return true;
            case OP_RETURN: markVisited(); return true;

            case OP_RUN_NODE: {
                const auto* n = requireOp(ins, 0, YsaOperandType::String);
                if (!n) return false;
                markVisited();
                return ExecuteNodeYsa(program, n->stringValue, interactive, state);
            }

            case OP_DETOUR_TO_NODE: {
                const auto* n = requireOp(ins, 0, YsaOperandType::String);
                if (!n) return false;
                if (!ExecuteNodeYsa(program, n->stringValue, interactive, state)) return false;
                ++ip; break;
            }

            case OP_PEEK_AND_RUN_NODE: {
                if (stack.empty()) { std::cerr << "Stack empty for peek_and_run_node\n"; return false; }
                const std::string target = stack.back().asString(); stack.pop_back();
                markVisited();
                return ExecuteNodeYsa(program, target, interactive, state);
            }

            case OP_PEEK_AND_DETOUR: {
                if (stack.empty()) { std::cerr << "Stack empty for peek_and_detour\n"; return false; }
                const std::string target = stack.back().asString(); stack.pop_back();
                if (!ExecuteNodeYsa(program, target, interactive, state)) return false;
                ++ip; break;
            }

            case OP_PUSH_STRING: {
                const auto* op = requireOp(ins, 0, YsaOperandType::String);
                if (!op) return false;
                stack.push_back(Value::fromString(op->stringValue)); ++ip; break;
            }

            case OP_PUSH_FLOAT: {
                const auto* op = requireOp(ins, 0, YsaOperandType::Float);
                if (!op) return false;
                stack.push_back(Value::fromFloat(op->floatValue)); ++ip; break;
            }

            case OP_PUSH_BOOL: {
                const auto* op = requireOp(ins, 0, YsaOperandType::Bool);
                if (!op) return false;
                stack.push_back(Value::fromBool(op->boolValue)); ++ip; break;
            }

            case OP_PUSH_VARIABLE: {
                const auto* op = requireOp(ins, 0, YsaOperandType::String);
                if (!op) return false;
                auto vit = state.variables.find(op->stringValue);
                stack.push_back(vit != state.variables.end() ? vit->second : Value::fromFloat(0.0f));
                ++ip; break;
            }

            case OP_STORE_VARIABLE: {
                const auto* op = requireOp(ins, 0, YsaOperandType::String);
                if (!op) return false;
                if (stack.empty()) { std::cerr << "Stack empty for store_variable\n"; return false; }
                state.variables[op->stringValue] = stack.back(); // peek - value stays on stack
                ++ip; break;
            }

            case OP_CALL_FUNCTION: {
                const auto* op = requireOp(ins, 0, YsaOperandType::String);
                if (!op) return false;
                // ysc always pushes arg count as a float immediately before call_func
                if (stack.empty()) { std::cerr << "Stack empty reading arg count for " << op->stringValue << "\n"; return false; }
                const int arity = static_cast<int>(stack.back().asFloat()); stack.pop_back();
                if (static_cast<int>(stack.size()) < arity) { std::cerr << "Not enough args for " << op->stringValue << "\n"; return false; }
                std::vector<Value> args;
                for (int a = 0; a < arity; ++a) { args.push_back(stack.back()); stack.pop_back(); }
                std::reverse(args.begin(), args.end());
                // Strip optional "Type." prefix (e.g. "Number.Add" -> "Add")
                std::string funcName = op->stringValue;
                const auto dotPos = funcName.rfind('.');
                if (dotPos != std::string::npos) funcName = funcName.substr(dotPos + 1);
                stack.push_back(CallFunction(funcName, std::move(args), state));
                ++ip; break;
            }

            case OP_ADD_SALIENCY_CANDIDATE: {
                const auto* cid   = requireOp(ins, 0, YsaOperandType::String);
                const auto* score = requireOp(ins, 1, YsaOperandType::U16);
                const auto* dest  = requireOp(ins, 2, YsaOperandType::U16);
                if (!cid || !score || !dest) return false;
                saliencyCandidates.push_back({cid->stringValue, score->u16Value, dest->u16Value});
                ++ip; break;
            }

            case OP_ADD_SALIENCY_CANDIDATE_FROM_NODE: {
                const auto* nop  = requireOp(ins, 0, YsaOperandType::String);
                const auto* dest = requireOp(ins, 1, YsaOperandType::U16);
                if (!nop || !dest) return false;
                const int score = state.visitedNodes.count(nop->stringValue) > 0 ? 0 : 1;
                saliencyCandidates.push_back({nop->stringValue, score, dest->u16Value});
                ++ip; break;
            }

            case OP_SELECT_SALIENCY_CANDIDATE: {
                if (saliencyCandidates.empty()) { std::cerr << "No saliency candidates\n"; return false; }
                auto best = std::max_element(saliencyCandidates.begin(), saliencyCandidates.end(),
                    [](const SaliencyCandidate& a, const SaliencyCandidate& b) { return a.complexityScore < b.complexityScore; });
                const int dest = best->destination;
                saliencyCandidates.clear();
                stack.push_back(Value::fromFloat(static_cast<float>(dest)));
                stack.push_back(Value::fromBool(true));
                ++ip; break;
            }

            default:
                std::cerr << "Unsupported YSA opcode " << (int)ins.opcode << " in " << nodeName << "\n";
                return false;
        }
    }
    // Covers only the natural fall-through case.
    markVisited();
    return true;
}

// ── .yarnc executor ───────────────────────────────────────────────────────────

bool ExecuteNode(const Yarn::Program& program, const std::string& nodeName,
                 const std::map<std::string, std::string>& lineTable,
                 bool interactive, RuntimeState& state);

bool ExecuteNode(const Yarn::Program& program, const std::string& nodeName,
                 const std::map<std::string, std::string>& lineTable,
                 bool interactive, RuntimeState& state) {
    auto nit = program.nodes().find(nodeName);
    if (nit == program.nodes().end()) { std::cerr << "Node not found: " << nodeName << "\n"; return false; }

    const Yarn::Node& node = nit->second;
    std::cout << "\n--- " << nodeName << " ---\n";

    std::vector<Value> stack;
    std::vector<OptionChoice> pendingOptions;
    std::vector<SaliencyCandidate> saliencyCandidates;
    int ip = 0;

    auto markVisited = [&]() {
        state.visitedNodes.insert(nodeName);
        state.visitCounts[nodeName]++;
    };

    while (ip >= 0 && ip < node.instructions_size()) {
        const Yarn::Instruction& ins = node.instructions(ip);

        if (ins.has_runline()) {
            const int sc = ins.runline().substitutioncount();
            auto subs = PopSubstitutions(stack, sc);
            std::cout << ApplySubstitutions(LookupLine(ins.runline().lineid(), lineTable), subs) << "\n";
            ++ip; continue;
        }
        if (ins.has_runcommand()) {
            const int sc = ins.runcommand().substitutioncount();
            auto subs = PopSubstitutions(stack, sc);
            std::cout << "[command] " << ApplySubstitutions(ins.runcommand().commandtext(), subs) << "\n";
            ++ip; continue;
        }
        if (ins.has_addoption()) {
            const auto& opt = ins.addoption();
            bool available = true;
            if (opt.hascondition()) {
                if (stack.empty()) { std::cerr << "Stack empty for conditional option\n"; return false; }
                available = stack.back().asBool(); stack.pop_back();
            }
            auto subs = PopSubstitutions(stack, opt.substitutioncount());
            pendingOptions.push_back({ApplySubstitutions(LookupLine(opt.lineid(), lineTable), subs), opt.destination(), available});
            ++ip; continue;
        }
        if (ins.has_showoptions()) {
            if (pendingOptions.empty()) { std::cerr << "No options to show\n"; return false; }
            std::cout << "Options:\n";
            const int sel = interactive ? PromptOptionSelection(pendingOptions) : AutoSelectOption(pendingOptions);
            if (sel < 0) { std::cerr << "No available options\n"; return false; }
            if (!interactive) std::cout << "  Auto-selecting: " << pendingOptions[static_cast<size_t>(sel)].resolvedText << "\n";
            const int dest = pendingOptions[static_cast<size_t>(sel)].destination;
            pendingOptions.clear();
            stack.push_back(Value::fromFloat(static_cast<float>(dest)));
            stack.push_back(Value::fromBool(true));
            ++ip; continue;
        }
        if (ins.has_jumpto())      { ip = ins.jumpto().destination(); continue; }
        if (ins.has_jumpiffalse()) {
            if (stack.empty()) { std::cerr << "Stack empty for jump_if_false\n"; return false; }
            const bool cond = stack.back().asBool(); // PEEK — value stays; pop at jump target cleans it
            ip = cond ? ip + 1 : ins.jumpiffalse().destination(); continue;
        }
        if (ins.has_peekandjump()) {
            if (stack.empty()) { std::cerr << "Stack empty for peek_and_jump\n"; return false; }
            ip = static_cast<int>(stack.back().asFloat()); continue;
        }
        if (ins.has_pop())    { if (!stack.empty()) stack.pop_back(); ++ip; continue; }
        if (ins.has_stop())   { std::cout << "[stop]\n"; markVisited(); return true; }
        if (ins.has_return_()) { markVisited(); return true; }

        if (ins.has_runnode()) {
            markVisited();
            return ExecuteNode(program, ins.runnode().nodename(), lineTable, interactive, state);
        }
        if (ins.has_detourtonode()) {
            if (!ExecuteNode(program, ins.detourtonode().nodename(), lineTable, interactive, state)) return false;
            ++ip; continue;
        }
        if (ins.has_peekandrunnode()) {
            if (stack.empty()) return false;
            const std::string target = stack.back().asString(); stack.pop_back();
            markVisited();
            return ExecuteNode(program, target, lineTable, interactive, state);
        }
        if (ins.has_peekanddetourtonode()) {
            if (stack.empty()) return false;
            const std::string target = stack.back().asString(); stack.pop_back();
            if (!ExecuteNode(program, target, lineTable, interactive, state)) return false;
            ++ip; continue;
        }

        if (ins.has_pushstring()) { stack.push_back(Value::fromString(ins.pushstring().value()));  ++ip; continue; }
        if (ins.has_pushfloat())  { stack.push_back(Value::fromFloat(ins.pushfloat().value()));    ++ip; continue; }
        if (ins.has_pushbool())   { stack.push_back(Value::fromBool(ins.pushbool().value()));      ++ip; continue; }

        if (ins.has_pushvariable()) {
            const std::string& varName = ins.pushvariable().variablename();
            auto vit = state.variables.find(varName);
            stack.push_back(vit != state.variables.end() ? vit->second : Value::fromFloat(0.0f));
            ++ip; continue;
        }
        if (ins.has_storevariable()) {
            if (!stack.empty()) state.variables[ins.storevariable().variablename()] = stack.back();
            ++ip; continue;
        }
        if (ins.has_callfunc()) {
            const std::string& fn = ins.callfunc().functionname();
            // ysc always pushes arg count as a float immediately before call_func
            if (stack.empty()) { std::cerr << "Stack empty reading arg count for " << fn << "\n"; return false; }
            const int arity = static_cast<int>(stack.back().asFloat()); stack.pop_back();
            if (static_cast<int>(stack.size()) < arity) { std::cerr << "Not enough args for " << fn << "\n"; return false; }
            std::vector<Value> args;
            for (int a = 0; a < arity; ++a) { args.push_back(stack.back()); stack.pop_back(); }
            std::reverse(args.begin(), args.end());
            // Strip optional "Type." prefix (e.g. "Number.Add" -> "Add")
            std::string funcName = fn;
            const auto dotPos = funcName.rfind('.');
            if (dotPos != std::string::npos) funcName = funcName.substr(dotPos + 1);
            stack.push_back(CallFunction(funcName, std::move(args), state));
            ++ip; continue;
        }

        if (ins.has_addsaliencycandidate()) {
            const auto& sc = ins.addsaliencycandidate();
            saliencyCandidates.push_back({sc.contentid(), sc.complexityscore(), sc.destination()});
            ++ip; continue;
        }
        if (ins.has_addsaliencycandidatefromnode()) {
            const auto& sc = ins.addsaliencycandidatefromnode();
            const int score = state.visitedNodes.count(sc.nodename()) > 0 ? 0 : 1;
            saliencyCandidates.push_back({sc.nodename(), score, sc.destination()});
            ++ip; continue;
        }
        if (ins.has_selectsaliencycandidate()) {
            if (saliencyCandidates.empty()) { std::cerr << "No saliency candidates\n"; return false; }
            auto best = std::max_element(saliencyCandidates.begin(), saliencyCandidates.end(),
                [](const SaliencyCandidate& a, const SaliencyCandidate& b) { return a.complexityScore < b.complexityScore; });
            const int dest = best->destination;
            saliencyCandidates.clear();
            stack.push_back(Value::fromFloat(static_cast<float>(dest)));
            stack.push_back(Value::fromBool(true));
            ++ip; continue;
        }

        std::cerr << "Unsupported .yarnc instruction in " << nodeName << "\n";
        return false;
    }
    // Covers only the natural fall-through case.
    markVisited();
    return true;
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    std::filesystem::path inputPath = std::filesystem::path("assets") / "sample_project" / "out" / "main.yarnc";
    std::filesystem::path linesPath = std::filesystem::path("assets") / "sample_project" / "out" / "main-Lines.csv";
    std::string startNode = "Start";
    bool interactiveMode = false;
    bool linesPathExplicit = false;

    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--interactive" || arg == "-i") { interactiveMode = true; continue; }
        if (arg == "--input"  && i+1 < argc) { inputPath = argv[++i]; continue; }
        if (arg == "--lines"  && i+1 < argc) { linesPath = argv[++i]; linesPathExplicit = true; continue; }
        if (arg == "--start"  && i+1 < argc) { startNode = argv[++i]; continue; }
        positional.push_back(arg);
    }
    if (!positional.empty()) inputPath = positional[0];
    const bool isYsa = inputPath.extension() == ".ysa";
    if (isYsa) {
        if (positional.size() > 1) startNode = positional[1];
    } else {
        if (positional.size() > 1) { linesPath = positional[1]; linesPathExplicit = true; }
        if (positional.size() > 2) startNode = positional[2];
    }

    if (isYsa) {
        YsaProgram ysaProgram;
        if (!LoadYsaProgramFromFile(inputPath, ysaProgram)) return 1;
        RuntimeState state;
        state.variables = ysaProgram.initialValues;
        if (!ExecuteNodeYsa(ysaProgram, startNode, interactiveMode, state)) return 1;
    } else {
        if (!linesPathExplicit) {
            // Prefer the adjacent modern ysc output (<stem>-Lines), fallback to legacy CSV name.
            const std::filesystem::path base = inputPath.parent_path() / (inputPath.stem().string() + "-Lines");
            const std::filesystem::path csv = base.string() + ".csv";
            if (std::filesystem::exists(base)) {
                linesPath = base;
            } else if (std::filesystem::exists(csv)) {
                linesPath = csv;
            }
        }

        Yarn::Program program;
        if (!LoadProgramFromFile(inputPath, program)) return 1;
        auto lineTable = LoadLineTable(linesPath);
        if (lineTable.empty()) std::cout << "No line table loaded from " << linesPath << "\n";

        // Seed variables from program initial_values
        RuntimeState state;
        for (const auto& [name, operand] : program.initial_values()) {
            if (operand.has_float_value())  state.variables[name] = Value::fromFloat(operand.float_value());
            else if (operand.has_bool_value())   state.variables[name] = Value::fromBool(operand.bool_value());
            else if (operand.has_string_value()) state.variables[name] = Value::fromString(operand.string_value());
        }
        if (!ExecuteNode(program, startNode, lineTable, interactiveMode, state)) return 1;
    }

    std::cout << "\nHarness completed successfully\n";
    return 0;
}
