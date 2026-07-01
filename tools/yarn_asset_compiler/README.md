# Yarn Asset Compiler

Host-side tool that converts YarnSpinner compiler output (`.yarnc` + `*-Lines.csv`) into a firmware-friendly binary format that does not require protobuf on-device.

## Native format: `YSA1` (v2)

All integers are little-endian.

1. Header
   - `magic[4]` = `YSA1`
   - `version:u16` = `2`
   - `node_count:u16`
   - `line_count:u32`
   - `initial_value_count:u16`
2. Nodes (`node_count` entries)
   - `node_name:str` (`u16 len + utf8 bytes`)
   - `instruction_count:u16`
   - For each instruction:
     - `instruction_size:u16`
     - `instruction_bytes[instruction_size]`
3. Line table (`line_count` entries, sorted by line id)
   - `line_id:str`
   - `line_text:str`
4. Initial values (`initial_value_count` entries, sorted by variable name)
   - `variable_name:str`
   - `value_type:u8` (`0=float`, `1=bool`, `2=string`)
   - value payload

Instruction encoding:

- `opcode:u8`
- `operand_count:u8`
- Repeated operands:
  - `operand_type:u8` (`0=u16`, `1=bool`, `2=string`, `3=float`)
  - operand payload

Supported opcodes:

- `1` run_line
- `2` run_command
- `3` add_option
- `4` show_options
- `5` jump_to
- `6` stop
- `7` run_node
- `8` jump_if_false
- `9` peek_and_jump
- `10` return
- `11` pop
- `12` push_string
- `13` push_float
- `14` push_bool
- `15` push_variable
- `16` store_variable
- `17` call_function
- `18` detour_to_node
- `19` peek_and_detour
- `20` peek_and_run_node
- `21` add_saliency_candidate
- `22` add_saliency_candidate_from_node
- `23` select_saliency_candidate

The compiler fails fast on unsupported instruction types to keep runtime behavior explicit.

## Usage

```powershell
python .\compile_yarn_assets.py `
  --yarnc ..\yarnspinner_harness\assets\sample_project\out\main.yarnc `
  --lines ..\yarnspinner_harness\assets\sample_project\out\main-Lines `
  --out ..\yarnspinner_harness\assets\sample_project\out\main.ysa
```

## Regenerating protobuf Python bindings

```powershell
python -m grpc_tools.protoc `
  --proto_path .\proto `
  --python_out .\generated `
  .\proto\yarn_spinner.proto
```
