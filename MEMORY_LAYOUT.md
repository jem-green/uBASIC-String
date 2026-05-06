# uBasic Memory Layout (Byte Level)

This document describes the exact in-memory layout used by the interpreter memory buffer.

Source definitions:
- `ubasic.h` constants and struct definitions
- `ubasic.c` runtime placement in `ubasic_load_program()`

## 1) Fixed low-memory control block

These offsets are absolute from the start of the provided memory buffer (`memory[0]`).

- `UBASIC_MEM_RESUME_OFFSET = 0`
  - bytes `0..3` (4 bytes): `int32_t resume_offset`
- `UBASIC_MEM_GOSUB_DEPTH_OFFSET = 4`
  - bytes `4..7` (4 bytes): `int32_t gosub_depth`
- `UBASIC_MEM_FOR_DEPTH_OFFSET = 8`
  - bytes `8..11` (4 bytes): `int32_t for_depth`
- `UBASIC_MEM_GOSUB_STACK_OFFSET = 12`
  - bytes `12..51` (40 bytes): `int32_t gosub_stack[10]`
- `UBASIC_MEM_FOR_STACK_OFFSET = 52`
  - bytes `52..99` (48 bytes): `for_state for_stack[4]`

With current `for_state` fields:
- `uint32_t line_after_for` (4)
- `int32_t for_variable_index` (4)
- `int32_t to` (4)

So `sizeof(for_state) = 12`

- `UBASIC_MEM_PUSH_DEPTH_OFFSET = 100`
  - bytes `100..103` (4 bytes): `int32_t push_depth`
- `UBASIC_MEM_PUSH_STACK_OFFSET = 104`
  - bytes `104..887` (784 bytes): `push_entry push_stack[16]`


With current `push_entry` fields:
- `int32_t type` (4)
- `VARIABLE_TYPE ival` (4, int32_t integer only)
- `char sval[41]` (41 bytes, where UBASIC_PUSH_STRING_LEN = 40)

With natural alignment, `sizeof(push_entry) = 48` and:
- `UBASIC_MEM_PROGRAM_OFFSET = 1000`

## 2) PUSH/POP stack region

- `UBASIC_MEM_PUSH_DEPTH_OFFSET = 100`
  - bytes `100..103` (4 bytes): `int32_t push_depth`
- `UBASIC_MEM_PUSH_STACK_OFFSET = 104`
  - bytes `104..999` (896 bytes): `push_entry push_stack[16]`

With `sizeof(push_entry) = 56`:
- `UBASIC_MEM_PROGRAM_OFFSET = 1000`

## 3) Program region

- Program starts at byte `1000`
- Program is stored as text bytes plus trailing NULL

If program length is `L` bytes (not counting NULL), then:
- Program occupies bytes `1000 .. (1000 + L)`
- `program_end = 1000 + L + 1`

## 4) Numeric variable region


`VARIABLE_TYPE` is `int32_t` (integer-only mode).

- `sizeof(VARIABLE_TYPE) = 4`
- `UBASIC_VARIABLE_COUNT = 26`
- `UBASIC_VARIABLES_SIZE = 26 * 4 = 104`

Variables are placed immediately after the NULL-terminated program:
- `variables_offset = program_end`
- Variable bytes: `variables_offset .. (variables_offset + 207)`

## 5) String state descriptor region

`ubasic_string_state_t` is packed (`#pragma pack(push, 1)`):
- `uint16_t pool_base` (2 bytes)
- `uint16_t pool_min` (2 bytes)
- `uint16_t var_off[26]` (26 entries, 2 bytes each = 52 bytes)

String-variable count in this table:
- `26` string variables (`a$`..`z$`)
- each `var_off[i]` stores one absolute offset into `memory[]`

Total:
- `UBASIC_STRING_STATE_SIZE = 56`


Placed immediately after numeric variables:
- `string_state_offset = variables_offset + 104`
- `string_state_end = string_state_offset + 56`

Descriptor byte range:
- `string_state_offset .. (string_state_end - 1)`

## 6) High-memory string pool (grows downward)

At load time:
- `pool_base = mem_capacity_bytes`
- `pool_min = string_state_end`

String data storage is absolute offsets into the same memory buffer.

The pool grows downward from the top of memory toward `pool_min`.

Usable string-pool bytes right after load:
- `mem_capacity_bytes - string_state_end`

## 7) Minimum required bytes

From `UBASIC_MIN_MEMORY_BYTES`:

- `UBASIC_MIN_MEMORY_BYTES = UBASIC_MEM_PROGRAM_OFFSET + 1 + UBASIC_HEAP_BYTES + UBASIC_VARIABLES_SIZE + UBASIC_STRING_STATE_SIZE`
- With current defaults (`UBASIC_HEAP_BYTES = 0`):
  - `UBASIC_MIN_MEMORY_BYTES = 1000 + 1 + 0 + 104 + 56 = 1161`

This minimum allows an empty program (just trailing NULL), control data, numeric variables, string descriptors, and the PUSH/POP stack, with zero free string-pool capacity.

## 8) Layout summary formula

Given:
- total memory capacity `M`
- program text length `L`

Then:
- Fixed control block: bytes `0..99`
- PUSH/POP stack: bytes `100..999`
- Program+NUL: bytes `1000..(1000+L)`
- Numeric vars: bytes `(1001+L)..(1104+L)`
- String state: bytes `(1105+L)..(1160+L)`
- String pool free area initially: bytes `(1161+L)..(M-1)` (used from top downward)

Constraint for successful load:
- `1161 + L <= M`
