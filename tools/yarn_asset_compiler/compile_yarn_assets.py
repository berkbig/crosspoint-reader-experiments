#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import struct
from pathlib import Path

from generated import yarn_spinner_pb2


MAGIC = b"YSA1"
FORMAT_VERSION = 2

OP_RUN_LINE = 1
OP_RUN_COMMAND = 2
OP_ADD_OPTION = 3
OP_SHOW_OPTIONS = 4
OP_JUMP_TO = 5
OP_STOP = 6
OP_RUN_NODE = 7
OP_JUMP_IF_FALSE = 8
OP_PEEK_AND_JUMP = 9
OP_RETURN = 10
OP_POP = 11
OP_PUSH_STRING = 12
OP_PUSH_FLOAT = 13
OP_PUSH_BOOL = 14
OP_PUSH_VARIABLE = 15
OP_STORE_VARIABLE = 16
OP_CALL_FUNCTION = 17
OP_DETOUR_TO_NODE = 18
OP_PEEK_AND_DETOUR = 19
OP_PEEK_AND_RUN_NODE = 20
OP_ADD_SALIENCY_CANDIDATE = 21
OP_ADD_SALIENCY_CANDIDATE_FROM_NODE = 22
OP_SELECT_SALIENCY_CANDIDATE = 23

TYPE_U16 = 0
TYPE_BOOL = 1
TYPE_STRING = 2
TYPE_FLOAT = 3


def encode_string(value: str) -> bytes:
    encoded = value.encode("utf-8")
    if len(encoded) > 0xFFFF:
        raise ValueError(f"String too long for u16 length prefix: {value[:80]}")
    return struct.pack("<H", len(encoded)) + encoded


def encode_operand_u16(value: int) -> bytes:
    if value < 0 or value > 0xFFFF:
        raise ValueError(f"u16 operand out of range: {value}")
    return bytes((TYPE_U16,)) + struct.pack("<H", value)


def encode_operand_bool(value: bool) -> bytes:
    return bytes((TYPE_BOOL, 1 if value else 0))


def encode_operand_string(value: str) -> bytes:
    return bytes((TYPE_STRING,)) + encode_string(value)


def encode_operand_float(value: float) -> bytes:
    return bytes((TYPE_FLOAT,)) + struct.pack("<f", value)


def encode_instruction(opcode: int, operands: list[bytes]) -> bytes:
    if len(operands) > 0xFF:
        raise ValueError("Too many operands in instruction")
    return bytes((opcode, len(operands))) + b"".join(operands)


def encode_initial_value(var_name: str, operand) -> bytes:
    result = encode_string(var_name)
    which = operand.WhichOneof("value")
    if which == "float_value":
        result += bytes((0,)) + struct.pack("<f", operand.float_value)
    elif which == "bool_value":
        result += bytes((1, 1 if operand.bool_value else 0))
    elif which == "string_value":
        result += bytes((2,)) + encode_string(operand.string_value)
    else:
        result += bytes((0,)) + struct.pack("<f", 0.0)
    return result


def convert_instruction(instruction: yarn_spinner_pb2.Instruction) -> bytes:
    kind = instruction.WhichOneof("InstructionType")
    if kind == "runLine":
        return encode_instruction(OP_RUN_LINE, [
            encode_operand_string(instruction.runLine.lineID),
            encode_operand_u16(instruction.runLine.substitutionCount),
        ])
    if kind == "runCommand":
        return encode_instruction(OP_RUN_COMMAND, [
            encode_operand_string(instruction.runCommand.commandText),
            encode_operand_u16(instruction.runCommand.substitutionCount),
        ])
    if kind == "addOption":
        return encode_instruction(OP_ADD_OPTION, [
            encode_operand_string(instruction.addOption.lineID),
            encode_operand_u16(instruction.addOption.destination),
            encode_operand_u16(instruction.addOption.substitutionCount),
            encode_operand_bool(instruction.addOption.hasCondition),
        ])
    if kind == "showOptions":
        return encode_instruction(OP_SHOW_OPTIONS, [])
    if kind == "jumpTo":
        return encode_instruction(OP_JUMP_TO, [encode_operand_u16(instruction.jumpTo.destination)])
    if kind == "jumpIfFalse":
        return encode_instruction(OP_JUMP_IF_FALSE, [encode_operand_u16(instruction.jumpIfFalse.destination)])
    if kind == "peekAndJump":
        return encode_instruction(OP_PEEK_AND_JUMP, [])
    if kind == "return":
        return encode_instruction(OP_RETURN, [])
    if kind == "pop":
        return encode_instruction(OP_POP, [])
    if kind == "stop":
        return encode_instruction(OP_STOP, [])
    if kind == "runNode":
        return encode_instruction(OP_RUN_NODE, [encode_operand_string(instruction.runNode.nodeName)])
    if kind == "pushString":
        return encode_instruction(OP_PUSH_STRING, [encode_operand_string(instruction.pushString.value)])
    if kind == "pushFloat":
        return encode_instruction(OP_PUSH_FLOAT, [encode_operand_float(instruction.pushFloat.value)])
    if kind == "pushBool":
        return encode_instruction(OP_PUSH_BOOL, [encode_operand_bool(instruction.pushBool.value)])
    if kind == "pushVariable":
        return encode_instruction(OP_PUSH_VARIABLE, [encode_operand_string(instruction.pushVariable.variableName)])
    if kind == "storeVariable":
        return encode_instruction(OP_STORE_VARIABLE, [encode_operand_string(instruction.storeVariable.variableName)])
    if kind == "callFunc":
        return encode_instruction(OP_CALL_FUNCTION, [encode_operand_string(instruction.callFunc.functionName)])
    if kind == "detourToNode":
        return encode_instruction(OP_DETOUR_TO_NODE, [encode_operand_string(instruction.detourToNode.nodeName)])
    if kind == "peekAndDetourToNode":
        return encode_instruction(OP_PEEK_AND_DETOUR, [])
    if kind == "peekAndRunNode":
        return encode_instruction(OP_PEEK_AND_RUN_NODE, [])
    if kind == "addSaliencyCandidate":
        sc = instruction.addSaliencyCandidate
        return encode_instruction(OP_ADD_SALIENCY_CANDIDATE, [
            encode_operand_string(sc.contentID),
            encode_operand_u16(sc.complexityScore),
            encode_operand_u16(sc.destination),
        ])
    if kind == "addSaliencyCandidateFromNode":
        sc = instruction.addSaliencyCandidateFromNode
        return encode_instruction(OP_ADD_SALIENCY_CANDIDATE_FROM_NODE, [
            encode_operand_string(sc.nodeName),
            encode_operand_u16(sc.destination),
        ])
    if kind == "selectSaliencyCandidate":
        return encode_instruction(OP_SELECT_SALIENCY_CANDIDATE, [])

    raise ValueError(f"Unsupported instruction type: {kind}")


def load_program(yarnc_path: Path) -> yarn_spinner_pb2.Program:
    program = yarn_spinner_pb2.Program()
    payload = yarnc_path.read_bytes()
    if not program.ParseFromString(payload):
        raise ValueError(f"Failed to parse .yarnc file: {yarnc_path}")
    return program


def load_lines(lines_csv_path: Path) -> dict[str, str]:
    line_map: dict[str, str] = {}
    with lines_csv_path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            line_id = (row.get("id") or "").strip()
            text = row.get("text") or ""
            if line_id:
                line_map[line_id] = text
    return line_map


def compile_asset(program: yarn_spinner_pb2.Program, line_map: dict[str, str]) -> bytes:
    node_items = sorted(program.nodes.items(), key=lambda pair: pair[0])
    line_items = sorted(line_map.items(), key=lambda pair: pair[0])
    initial_value_items = sorted(program.initial_values.items(), key=lambda pair: pair[0])

    output = bytearray()
    output += MAGIC
    output += struct.pack("<H", FORMAT_VERSION)
    output += struct.pack("<H", len(node_items))
    output += struct.pack("<I", len(line_items))
    output += struct.pack("<H", len(initial_value_items))  # v2: initial value count

    for node_name, node in node_items:
        output += encode_string(node_name)
        output += struct.pack("<H", len(node.instructions))
        for instruction in node.instructions:
            encoded = convert_instruction(instruction)
            if len(encoded) > 0xFFFF:
                raise ValueError(f"Instruction too large in node {node_name}")
            output += struct.pack("<H", len(encoded))
            output += encoded

    for line_id, text in line_items:
        output += encode_string(line_id)
        output += encode_string(text)

    for var_name, operand in initial_value_items:
        output += encode_initial_value(var_name, operand)

    return bytes(output)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compile .yarnc to YSA1 native format.")
    parser.add_argument("--yarnc", required=True, type=Path, help="Input .yarnc file path")
    parser.add_argument("--lines", required=True, type=Path, help="Input lines CSV path")
    parser.add_argument("--out", required=True, type=Path, help="Output .ysa file path")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    program = load_program(args.yarnc)
    line_map = load_lines(args.lines)
    compiled = compile_asset(program, line_map)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(compiled)
    print(
        f"Wrote {args.out} ({len(compiled)} bytes, "
        f"{len(program.nodes)} nodes, {len(line_map)} lines, "
        f"{len(program.initial_values)} initial values)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
