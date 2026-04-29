/*
 * Copyright (c) 2006, Adam Dunkels
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
 
 /*
 * Modified to support simple string variables and functions by David Mitchell
 * November 2008.
 * Changes and additions are marked 'string additions' throughout
 */


#ifndef __UBASIC_H__
#define __UBASIC_H__

#include <stddef.h>
#include <stdint.h>

#include "vartype.h"

typedef VARIABLE_TYPE (*peek_func)(VARIABLE_TYPE);
typedef void (*poke_func)(VARIABLE_TYPE, VARIABLE_TYPE);
typedef void (*out_func)(const char *);
typedef void (*put_func)(VARIABLE_TYPE);
typedef VARIABLE_TYPE (*in_func)(void);
typedef VARIABLE_TYPE (*get_func)(void);

/*
 * Single buffer layout - Classic 8-bit BASIC style:
 *   LOW MEMORY:
 *     int32_t resume_offset at offset 0 (tokenizer position for resume)
 *     int32_t gosub_depth at offset 4
 *     int32_t for_depth at offset 8
 *     int32_t gosub_stack[UBASIC_MAX_GOSUB_STACK_DEPTH]
 *     struct for_state for_stack[UBASIC_MAX_FOR_STACK_DEPTH]
 *     program bytes (NUL-terminated)
 *     VARIABLE_TYPE variables[UBASIC_VARIABLE_COUNT]  (a-z integer variables)
 *     ubasic_string_state_t  (pool_base, pool_min + var_off[26] string descriptors)
 *   
 *   DYNAMIC SPACE (string pool grows down from high memory during runtime)
 *   
 *   HIGH MEMORY (grows down from top):
 *   char string_pool[]  (variable size, grows DOWN during runtime)
 *
 * String pool is reset on program load and grows downward during execution.
 * UBASIC_STRING_POOL_SIZE is now ignored (pool uses all available space).
 * Scratch buffer must still be >= 4000 for str_pool_compact.
 */
#define UBASIC_STRING_VAR_MAXLEN 255
#ifndef UBASIC_STRING_POOL_SIZE
#define UBASIC_STRING_POOL_SIZE 1024
#endif
#define UBASIC_MAX_GOSUB_STACK_DEPTH 10
#define UBASIC_MAX_FOR_STACK_DEPTH 4
#define UBASIC_VARIABLE_COUNT 26

#ifndef UBASIC_HEAP_BYTES
#define UBASIC_HEAP_BYTES 0
#endif

typedef struct for_state {
  int32_t line_after_for;
  int32_t for_variable_index; /* 0..26 (a-z) */
  int32_t to;
} for_state;

/* Control state at low addresses */
#define UBASIC_MEM_RESUME_OFFSET      0
#define UBASIC_MEM_GOSUB_DEPTH_OFFSET 4
#define UBASIC_MEM_FOR_DEPTH_OFFSET   8
#define UBASIC_MEM_GOSUB_STACK_OFFSET 12
#define UBASIC_MEM_FOR_STACK_OFFSET \
  (UBASIC_MEM_GOSUB_STACK_OFFSET + UBASIC_MAX_GOSUB_STACK_DEPTH * (int)sizeof(int32_t))
/* Program starts immediately after control structures */
#define UBASIC_MEM_PROGRAM_OFFSET \
  (UBASIC_MEM_FOR_STACK_OFFSET + UBASIC_MAX_FOR_STACK_DEPTH * (int)sizeof(for_state))

/* Variables and string state in LOW memory after program (calculated at runtime) */
#define UBASIC_VARIABLES_SIZE (UBASIC_VARIABLE_COUNT * (int)sizeof(VARIABLE_TYPE))
#define UBASIC_STRING_STATE_SIZE ((int)sizeof(ubasic_string_state_t))

/* These offsets are calculated in ubasic_init and ubasic_load_program */
/* variables_mem = memory + (program_end) */
/* string_state  = memory + (program_end + UBASIC_VARIABLES_SIZE) */
/* string pool grows DOWN from HIGH memory toward string_state */

#pragma pack(push, 1)
typedef struct {
  uint16_t pool_base;    /* Current top of string pool (grows DOWN) */
  uint16_t pool_min;     /* Minimum address (below this is program) */
  uint16_t var_off[UBASIC_VARIABLE_COUNT];  /* Absolute offsets in memory[] */
} ubasic_string_state_t;
#pragma pack(pop)

#define UBASIC_MIN_MEMORY_BYTES \
  (UBASIC_MEM_PROGRAM_OFFSET + 1u + UBASIC_HEAP_BYTES + UBASIC_VARIABLES_SIZE + UBASIC_STRING_STATE_SIZE)

// Possibly public

void ubasic_init(uint8_t *memory, uint32_t memory_bytes);  /* Minimal init for snapshot restore */
void ubasic_reset(void);  /* Clears variables */
void ubasic_run(void);
int ubasic_finished(void);
void ubasic_load_program(const char *program); /* Preserves vars */

// string addition
char* get_stringvariable(int);
void set_stringvariable(int, char *);
// end of string addition

/* I/O function setter */
void ubasic_set_out_function(out_func func);
void ubasic_set_put_function(put_func func);
void ubasic_set_in_function(in_func func);
void ubasic_set_get_function(get_func func);

#endif /* __UBASIC_H__ */
