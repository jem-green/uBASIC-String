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

So `sizeof(for_state) = 12` and:
- `UBASIC_MEM_PROGRAM_OFFSET = 100`

## 2) Program region

- Program starts at byte `100`
- Program is stored as text bytes plus trailing NULL

If program length is `L` bytes (not counting NULL), then:
- Program occupies bytes `100 .. (100 + L)`
- `program_end = 100 + L + 1`

## 3) Numeric variable region

`VARIABLE_TYPE` is currently `int32_t`.

- `sizeof(VARIABLE_TYPE) = 4`
- `UBASIC_VARIABLE_COUNT = 26`
- `UBASIC_VARIABLES_SIZE = 26 * 4 = 104`

Variables are placed immediately after the NULL-terminated program:
- `variables_offset = program_end`
- Variable bytes: `variables_offset .. (variables_offset + 103)`

## 4) String state descriptor region

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

## 5) High-memory string pool (grows downward)

At load time:
- `pool_base = mem_capacity_bytes`
- `pool_min = string_state_end`

String data storage is absolute offsets into the same memory buffer.

The pool grows downward from the top of memory toward `pool_min`.

Usable string-pool bytes right after load:
- `mem_capacity_bytes - string_state_end`

## 6) Minimum required bytes

From `UBASIC_MIN_MEMORY_BYTES`:

- `UBASIC_MIN_MEMORY_BYTES = UBASIC_MEM_PROGRAM_OFFSET + 1 + UBASIC_HEAP_BYTES + UBASIC_VARIABLES_SIZE + UBASIC_STRING_STATE_SIZE`
- With current defaults (`UBASIC_HEAP_BYTES = 0`):
  - `UBASIC_MIN_MEMORY_BYTES = 100 + 1 + 0 + 104 + 56 = 261`

This minimum allows an empty program (just trailing NULL), control data, numeric variables, and string descriptors, with zero free string-pool capacity.

## 7) Layout summary formula

Given:
- total memory capacity `M`
- program text length `L`

Then:
- Fixed control block: bytes `0..99`
- Program+NUL: bytes `100..(100+L)`
- Numeric vars: bytes `(101+L)..(204+L)`
- String state: bytes `(205+L)..(260+L)`
- String pool free area initially: bytes `(261+L)..(M-1)` (used from top downward)

Constraint for successful load:
- `261 + L <= M`
