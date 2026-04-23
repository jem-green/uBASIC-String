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
 
#define DEBUG 0
#define VERBOSE 0

#if DEBUG
#define DEBUG_PRINTF(...)  printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif


#include "ubasic.h"
#include "tokenizer.h"

#include <stdio.h> /* printf() */
#include <stdlib.h> /* exit() */
#include <string.h> /* strlen(), memcpy(), sprintf() */
#include <stdint.h>

// Probably private

typedef VARIABLE_TYPE (*peek_func)(VARIABLE_TYPE);
typedef void (*poke_func)(VARIABLE_TYPE, VARIABLE_TYPE);
static VARIABLE_TYPE expr(void);
static void line_statement(void);
static void statement(void);
static void index_free(void);
static void reset_control_state(void);
static VARIABLE_TYPE expr(void);
void set_variable(int varum, VARIABLE_TYPE value);
VARIABLE_TYPE get_variable(int varnum);
static void gosub_push(int32_t line_num);
static int32_t gosub_pop(void);
static void for_push(for_state state);
static for_state for_pop(void);

static char const *program_ptr;
#define MAX_STRINGLEN 40
static char string[MAX_STRINGLEN];

static uint8_t *mem_base = NULL;
static size_t mem_capacity_bytes = 0;
static int32_t *resume_offset_cell = NULL;
static int32_t *gosub_depth_cell = NULL;
static int32_t *for_depth_cell = NULL;
static int32_t *gosub_stack_mem = NULL;
static for_state *for_stack = NULL;
static VARIABLE_TYPE *variables_mem = NULL;

// string additions
#define MAX_STRINGVARLEN UBASIC_STRING_VAR_MAXLEN
#define MAX_BUFFERLEN    4000
#if UBASIC_STRING_POOL_SIZE > MAX_BUFFERLEN
#error UBASIC_STRING_POOL_SIZE must not exceed MAX_BUFFERLEN (str_pool_compact scratch)
#endif
static char stringbuffer[MAX_BUFFERLEN];
static int freebufptr = 0;
#define MAX_SVARNUM UBASIC_VARIABLE_COUNT
// end of string additions

struct line_index {
  int line_number;
  char const *program_text_position;
  struct line_index *next;
};

struct line_index *line_index_head = NULL;
struct line_index *line_index_current = NULL;


static int ended;

static VARIABLE_TYPE expr(void);
static void line_statement(void);
static void statement(void);

static void index_free(void);

peek_func peek_function = NULL;
poke_func poke_function = NULL;

// string additions
static char nullstring[] = "\0"; 
static void  var_init(void);
static char* sexpr(void);
static char* scpy(const char *);
static char* sconcat(const char *, const char *);
static char* sleft(const char *, int); 
static char* sright(const char *,int);
static char* smid(const char *, int, int);
static char* sstr(int);
static char* schr(int);
static int sinstr(int, const char*, const char*);
static ubasic_string_state_t *str_state(void);
static char *str_pool_data(void);
static void str_pool_compact(void);
// end of string additions

/*---------------------------------------------------------------------------*/
// Public functions
/*---------------------------------------------------------------------------*/
void ubasic_init(uint8_t *memory, uint32_t memory_bytes) {
  size_t variables_offset;
  size_t string_state_offset;
  
  mem_base = memory;
  mem_capacity_bytes = memory_bytes;
  resume_offset_cell = (int32_t *)(memory + UBASIC_MEM_RESUME_OFFSET);
  gosub_depth_cell = (int32_t *)(memory + UBASIC_MEM_GOSUB_DEPTH_OFFSET);
  for_depth_cell = (int32_t *)(memory + UBASIC_MEM_FOR_DEPTH_OFFSET);
  gosub_stack_mem = (int32_t *)(memory + UBASIC_MEM_GOSUB_STACK_OFFSET);
  for_stack = (for_state *)(memory + UBASIC_MEM_FOR_STACK_OFFSET);
  
  /* Variables at TOP of memory */
  variables_offset = memory_bytes - UBASIC_VARIABLES_SIZE;
  variables_mem = (VARIABLE_TYPE *)(memory + variables_offset);
  memset(variables_mem, 0, UBASIC_VARIABLE_COUNT * sizeof(VARIABLE_TYPE));
  
  /* String state just below variables */
  string_state_offset = variables_offset - UBASIC_STRING_STATE_SIZE;
  
  /* Initialize string state */
  ubasic_string_state_t *str_st = (ubasic_string_state_t *)(memory + string_state_offset);
  str_st->pool_base = (uint16_t)string_state_offset;  /* Start at top */
  str_st->pool_min = UBASIC_MEM_PROGRAM_OFFSET + 1;   /* Can't grow below program start */
  memset(str_st->var_off, 0, sizeof(str_st->var_off));
  
  program_ptr = (char const *)(memory + UBASIC_MEM_PROGRAM_OFFSET);
  memory[UBASIC_MEM_PROGRAM_OFFSET] = '\0';
  index_free();
  tokenizer_init(program_ptr);
  #if VERBOSE
    DEBUG_PRINTF("ubasic_init: Variables at offset %zu (top of memory).\n", variables_offset);
    DEBUG_PRINTF("ubasic_init: String state at offset %zu.\n", string_state_offset);
    DEBUG_PRINTF("ubasic_init: String pool space: %zu bytes.\n", 
                 string_state_offset - UBASIC_MEM_PROGRAM_OFFSET - 1);
  #endif
}
/*---------------------------------------------------------------------------*/
void ubasic_init_peek_poke(uint8_t *memory, uint32_t memory_bytes,peek_func peek, poke_func poke) {
  ubasic_init(memory, memory_bytes);
  peek_function = peek;
  poke_function = poke;
  #if VERBOSE
    DEBUG_PRINTF("ubasic_init_peek_poke: Initializing uBasic with peek/poke.\n");
  #endif
}
/*---------------------------------------------------------------------------*/
static void reset_control_state(void) {
  index_free();
  tokenizer_reset();
  if (resume_offset_cell != NULL) {
    *resume_offset_cell = 0;
  }
  if (gosub_depth_cell != NULL) {
    *gosub_depth_cell = 0;
  }
  if (for_depth_cell != NULL) {
    *for_depth_cell = 0;
  }
  if (gosub_stack_mem != NULL) {
    memset(gosub_stack_mem, 0, UBASIC_MAX_GOSUB_STACK_DEPTH * sizeof(int32_t));
  }
  if (for_stack != NULL) {
    memset(for_stack, 0, UBASIC_MAX_FOR_STACK_DEPTH * sizeof(for_state));
  }
  ended = 0;
}
/*---------------------------------------------------------------------------*/
void ubasic_reset(void) {
  index_free();
  tokenizer_reset();
  if (gosub_depth_cell != NULL) {
    *gosub_depth_cell = 0;
  }
  if (for_depth_cell != NULL) {
    *for_depth_cell = 0;
  }
  ended = 0;

  #if VERBOSE
    DEBUG_PRINTF("ubasic_reset: Full reset including variables.\n");
  #endif
}
/*---------------------------------------------------------------------------*/
void ubasic_load_program(const char *program) {
  size_t len;
  size_t available_program_bytes;
  size_t program_end;
  size_t variables_offset;
  size_t string_state_offset;
  ubasic_string_state_t *str_st;

  if (mem_base == NULL) {
    DEBUG_PRINTF("ubasic_load_program: call ubasic_init first.\n");
    return;
  }
  
  /* Reset control state but PRESERVE VARIABLES */
  reset_control_state();
  
  len = strlen(program);
  
  /* Calculate where things are in memory */
  variables_offset = mem_capacity_bytes - UBASIC_VARIABLES_SIZE;
  string_state_offset = variables_offset - UBASIC_STRING_STATE_SIZE;
  program_end = UBASIC_MEM_PROGRAM_OFFSET + len + 1;
  
  /* Check program fits */
  if (program_end > string_state_offset) {
    DEBUG_PRINTF("ubasic_load_program: program too large (%u bytes, max %u).\n", 
                 (unsigned)len, (unsigned)(string_state_offset - UBASIC_MEM_PROGRAM_OFFSET - 1));
    return;
  }
  
  /* Load program */
  memcpy(mem_base + UBASIC_MEM_PROGRAM_OFFSET, program, len + 1);
  program_ptr = (char const *)(mem_base + UBASIC_MEM_PROGRAM_OFFSET);
  tokenizer_init(program_ptr);
  
  /* Reset string pool - it grows DOWN from string_state_offset toward program_end */
  str_st = (ubasic_string_state_t *)(mem_base + string_state_offset);
  str_st->pool_base = (uint16_t)string_state_offset;  /* Start at top */
  str_st->pool_min = (uint16_t)program_end;            /* Can't grow below program */
  memset(str_st->var_off, 0, sizeof(str_st->var_off));
  
  #if VERBOSE
    DEBUG_PRINTF("ubasic_load_program: Loaded %u bytes.\n", (unsigned)len);
    DEBUG_PRINTF("ubasic_load_program: String pool space: %u bytes.\n", 
                 (unsigned)(string_state_offset - program_end));
  #endif
}
/*---------------------------------------------------------------------------*/
int ubasic_finished(void){
  return ended || tokenizer_finished();
}
/*---------------------------------------------------------------------------*/
void ubasic_run(void){
  if(tokenizer_finished()) {
    #if VERBOSE
      DEBUG_PRINTF("ubasic_run: Program finished.\n");
    #endif
    return;
  }
  // string additions: repack string pool in memory[], then discard expr temporaries
  str_pool_compact();
  freebufptr = 0;
  // end of string additions

  line_statement();
}
/*---------------------------------------------------------------------------*/
static void save_position(void) {
  const char *current_pos = tokenizer_pos();
  const char *start_pos = tokenizer_start();
  int32_t offset = (int32_t)(current_pos - start_pos);
  
  if (resume_offset_cell != NULL) {
    *resume_offset_cell = offset;
    #if VERBOSE
      DEBUG_PRINTF("save_position: Saved offset %d.\n", offset);
    #endif
  }
}
/*---------------------------------------------------------------------------*/
void ubasic_resume(void) {
  int32_t offset;
  const char *start_pos;
  const char *resume_pos;
  
  if (resume_offset_cell == NULL || program_ptr == NULL) {
    DEBUG_PRINTF("ubasic_resume: Not initialized.\n");
    return;
  }
  
  offset = *resume_offset_cell;
  start_pos = tokenizer_start();
  resume_pos = start_pos + offset;
  
  tokenizer_goto(resume_pos);
  ended = 0;
  
  #if VERBOSE
    DEBUG_PRINTF("ubasic_resume: Resumed from offset %d.\n", offset);
  #endif
}
/*---------------------------------------------------------------------------*/
// Private functions
/*---------------------------------------------------------------------------*/
static void accept(int token){
  if(token != tokenizer_token()) {
    DEBUG_PRINTF("accept: Token not what was expected (expected '%s', got %s).\n",
    tokenizer_token_name(token),
    tokenizer_token_name(tokenizer_token()));
    tokenizer_error_print();
    exit(1);
  }
  DEBUG_PRINTF("accept: Expected '%s', got it.\n", tokenizer_token_name(token));
  tokenizer_next();
}
/*---------------------------------------------------------------------------*/
// string additions
/*---------------------------------------------------------------------------*/
static ubasic_string_state_t *str_state(void) {
  if (mem_base == NULL || mem_capacity_bytes == 0) {
    return NULL;
  }
  size_t variables_offset = mem_capacity_bytes - UBASIC_VARIABLES_SIZE;
  size_t string_state_offset = variables_offset - UBASIC_STRING_STATE_SIZE;
  return (ubasic_string_state_t *)(mem_base + string_state_offset);
}

static char *str_pool_data(void) {
  /* String pool is in the dynamic region, accessed via absolute offsets */
  return (char *)mem_base;
}

static void str_pool_compact(void) {
  ubasic_string_state_t *h;
  uint16_t old_off[UBASIC_VARIABLE_COUNT];
  uint16_t old_base;
  uint16_t string_state_offset;
  uint16_t newpos;
  int i;
  size_t total_size;

  if (mem_base == NULL) {
    return;
  }
  h = str_state();
  if (h == NULL) {
    return;
  }
  
  /* Calculate string state offset */
  string_state_offset = (uint16_t)(mem_capacity_bytes - UBASIC_VARIABLES_SIZE - UBASIC_STRING_STATE_SIZE);
  
  /* Save old offsets and copy all active strings to scratch buffer */
  memcpy(old_off, h->var_off, sizeof(old_off));
  old_base = h->pool_base;
  
  /* Copy active string data to scratch buffer */
  if (old_base < string_state_offset) {
    size_t used = string_state_offset - old_base;
    if (used > MAX_BUFFERLEN) {
      ended = 1;
      return;
    }
    memcpy(stringbuffer, mem_base + old_base, used);
  }
  
  /* Repack strings from TOP down (just below string_state) */
  newpos = string_state_offset;
  
  for (i = 0; i < UBASIC_VARIABLE_COUNT; i++) {
    uint16_t o = old_off[i];
    const char *src;
    size_t len;

    if (o == 0) {
      h->var_off[i] = 0;
      continue;
    }
    
    /* Validate old offset */
    if (o < old_base || o >= string_state_offset) {
      h->var_off[i] = 0;
      continue;
    }
    
    /* Get string from scratch buffer */
    src = stringbuffer + (o - old_base);
    len = strlen(src);
    if (len > (size_t)MAX_STRINGVARLEN) {
      len = (size_t)MAX_STRINGVARLEN;
    }
    
    /* Check if we have space (growing down from newpos) */
    if (newpos < h->pool_min + len + 1) {
      DEBUG_PRINTF("str_pool_compact: Out of memory during compaction.\n");
      ended = 1;
      return;
    }
    
    /* Allocate downward */
    newpos -= (uint16_t)(len + 1);
    memcpy(mem_base + newpos, src, len);
    mem_base[newpos + len] = '\0';
    h->var_off[i] = newpos;
  }
  
  h->pool_base = newpos;
  
  #if VERBOSE
    DEBUG_PRINTF("str_pool_compact: Compacted strings, new base=%u, freed %u bytes.\n",
                 (unsigned)newpos, (unsigned)(newpos - old_base));
  #endif
}

static void var_init() {
   int i;
   ubasic_string_state_t *h;
   size_t string_state_offset;

   freebufptr = 0;
   for (i=0; i<UBASIC_VARIABLE_COUNT; i++) 
      set_variable(i, 0);
   if (mem_base != NULL && mem_capacity_bytes > 0) {
     h = str_state();
     if (h != NULL) {
       string_state_offset = mem_capacity_bytes - UBASIC_VARIABLES_SIZE - UBASIC_STRING_STATE_SIZE;
       h->pool_base = (uint16_t)string_state_offset;
       h->pool_min = UBASIC_MEM_PROGRAM_OFFSET + 1;
       memset(h->var_off, 0, sizeof(h->var_off));
     }
   }
}
/*---------------------------------------------------------------------------*/
static int string_space_check(int l) {
   // returns true if not enough room for new string
   int i;
   i = ((MAX_BUFFERLEN - freebufptr) <= (l + 2)); // +2 to play it safe
   if (i) {
      ended = 1;
   }
   return i;
}
/*---------------------------------------------------------------------------*/
static char* scpy(const char *s1) { // return a copy of s1
   int bp = freebufptr;
   int l;
   l = strlen(s1);
   if (string_space_check(l)) 
      return nullstring;
   strcpy(stringbuffer+bp, s1);
   freebufptr = bp + l + 1;
   return stringbuffer+bp;
}
/*---------------------------------------------------------------------------*/
static char* sconcat(const char *s1, const char*s2) { // return the concatenation of s1 and s2
   int bp = freebufptr;   
   int rp = bp;
   int l1, l2;
   l1 = strlen(s1);
   l2 = strlen(s2);
   if (string_space_check(l1+l2))
      return nullstring;
   strcpy((stringbuffer+bp), s1);
   bp += l1;
   if (l1 == MAX_STRINGVARLEN) {
      freebufptr = bp + 1;
	  return (stringbuffer + rp); 
   }
   l2 = strlen(s2);
   strcpy((stringbuffer+bp), s2);
   if (l1 + l2 > MAX_STRINGVARLEN) {
      l2 = MAX_STRINGVARLEN - l1;
	  // truncate
	  *(stringbuffer + bp + l2) = '\0';
   }   
   freebufptr = bp + l2 + 1;
   return (stringbuffer+rp);   
}
/*---------------------------------------------------------------------------*/
static char* sleft(const char *s1, int l) { // return the left l chars of s1
   int bp = freebufptr;
   int rp = bp;
   int i;
   if (l<1) 
     return scpy(nullstring);
   if (string_space_check(l))
      return nullstring;
   if (strlen(s1) <=l) {
      return scpy(s1);
   } else {
      for (i=0; i<l; i++) {
	     *(stringbuffer +bp) = *s1;
		 bp++;
		 s1++;
	  }
	  *(stringbuffer + bp) = '\0';
	  freebufptr = bp+1;
	
   }
   return stringbuffer + rp;
}
/*---------------------------------------------------------------------------*/
static char* sright(const char *s1, int l) { // return the right l chars of s1
   int bp = freebufptr;
   int rp = bp;
   int i, j;
   j = strlen(s1);
   if (l<1) 
     return scpy(nullstring);
   if (string_space_check(l))
      return nullstring;
   if (j <= l) {
      return scpy(s1);
   } else {
      for (i=0; i<l; i++) {
	     *(stringbuffer + bp) = *(s1 + j-l);
		 bp++;
		 s1++;
	  }
	  *(stringbuffer + bp) = '\0';
	  freebufptr = bp+1;
	
   }
   return stringbuffer + rp;
}
/*---------------------------------------------------------------------------*/
static char* smid(const char *s1, int l1, int l2) { // return the l2 chars of s1 starting at offset l1
   int bp = freebufptr;
   int rp = bp;
   int i, j;
   j = strlen(s1);
   if (l2<1 || l1>j) 
      return scpy(nullstring);
   if (string_space_check(l2))
      return nullstring;
   if (l2 > j-l1)
     l2 = j-l1;
   for (i=l1; i<l1+l2; i++) {
      *(stringbuffer + bp) = *(s1 + l1 -1);
      bp++;
      s1++;
   }
   *(stringbuffer + bp) = '\0';
   freebufptr = bp+1;
   return stringbuffer + rp;
}
/*---------------------------------------------------------------------------*/
static char* sstr(int j) { // return the integer j as a string
   int bp = freebufptr;
   int rp = bp;
   if (string_space_check(10))
      return nullstring;
   sprintf((stringbuffer+bp),"%d",j);
   freebufptr = bp + strlen(stringbuffer+bp) + 1;
   return stringbuffer + rp;
}
/*---------------------------------------------------------------------------*/
static char* schr(int j) { // return the character whose ASCII code is j
   int bp = freebufptr;
   int rp = bp;
   
   if (string_space_check(1))
      return nullstring;
   sprintf((stringbuffer+bp),"%c",j);
   
   freebufptr = bp + 2;
   return stringbuffer + rp;
}
/*---------------------------------------------------------------------------*/
static int sinstr(int j, const char *s, const char *s1) { // return the position of s1 in s (or 0) 
   char *p;
   p = strstr(s+j, s1);
   if (p == NULL)
      return 0;
   return (p - s + 1);
}
/*---------------------------------------------------------------------------*/
static char* sfactor() { // string form of factor
   char *r, *s;
   int i, j;
   switch(tokenizer_token()) {
       case TOKENIZER_LEFTPAREN:
	      accept(TOKENIZER_LEFTPAREN);
		  r = sexpr();
		  accept(TOKENIZER_RIGHTPAREN);
		  break;
	   case TOKENIZER_STRING:
	      tokenizer_string(string, sizeof(string));
		  r= scpy(string);
  	      accept(TOKENIZER_STRING);
	      break;
 	case TOKENIZER_LEFT$:
	      accept(TOKENIZER_LEFT$);
		  accept(TOKENIZER_LEFTPAREN);
          s = sexpr();
		  accept(TOKENIZER_COMMA);
		  i = expr();
		  r = sleft(s,i);
		  accept(TOKENIZER_RIGHTPAREN);
          break;
	case TOKENIZER_RIGHT$:
	      accept(TOKENIZER_RIGHT$);
		  accept(TOKENIZER_LEFTPAREN);
		  s = sexpr();
		  accept(TOKENIZER_COMMA);
		  i = expr();
		  r = sright(s,i);
		  accept(TOKENIZER_RIGHTPAREN);
          break;
	case TOKENIZER_MID$:
	      accept(TOKENIZER_MID$);
		  accept(TOKENIZER_LEFTPAREN);
		  s = sexpr();
		  accept(TOKENIZER_COMMA);
		  i = expr();
		  if (tokenizer_token() == TOKENIZER_COMMA) {
		     accept(TOKENIZER_COMMA);
			 j = expr();
		  } else {
		     j = 999; // ensure we get all of it
		  }
		  r = smid(s,i,j);
		  accept(TOKENIZER_RIGHTPAREN);
          break;
    case TOKENIZER_STR$:
	      accept(TOKENIZER_STR$);
	      j = expr();
		  r = sstr(j);
	      break;
	case TOKENIZER_CHR$:
	     accept(TOKENIZER_CHR$);
		 j = expr();
		 if (j<0 || j>255)
		    j = 0;
		 r = schr(j);
		 break;
	default:	  
		  r = get_stringvariable(tokenizer_variable_num());
	      accept(TOKENIZER_STRINGVARIABLE);
	}
   return r;
}
/*---------------------------------------------------------------------------*/
static char* sexpr(void) { // string form of expr
   char *s1, *s2;
   int op;
   s1 = sfactor();
   op = tokenizer_token();
   DEBUG_PRINTF("sexpr s1= '%s' op= %d\n", s1, op);  
   while(op == TOKENIZER_PLUS) {
      tokenizer_next();
	  s2 = sfactor();
	  s1 = sconcat(s1,s2);
	  op = tokenizer_token();
   }
   DEBUG_PRINTF("sexpr returning s1= '%s'\n", s1);  

   return s1;
}
/*---------------------------------------------------------------------------*/
static int slogexpr() { // string logical expression
   char *s1, *s2;
   int op;
   int r = 0;
   s1 = sexpr();
   op = tokenizer_token();
   tokenizer_next();
   switch(op) {
      case TOKENIZER_EQ:
	     s2 = sexpr();
	     r = (strcmp(s1,s2) == 0);
		 break;
   }
   return r;
}
/*---------------------------------------------------------------------------*/
// end of string additions
/*---------------------------------------------------------------------------*/
static int varfactor(void) {
  int r;
  DEBUG_PRINTF("varfactor: obtaining %d from variable %d.\n", get_variable(tokenizer_variable_num()), tokenizer_variable_num());
  r = get_variable(tokenizer_variable_num());
  accept(TOKENIZER_VARIABLE);
  return r;
}
/*---------------------------------------------------------------------------*/
static int factor(void) {
  int r;
 // string function additions
  int j;
  char *s, *s1;
  
  DEBUG_PRINTF("factor: token '%s'.\n", tokenizer_token_name(tokenizer_token()));
  switch(tokenizer_token()) {
     case TOKENIZER_LEN:
      accept(TOKENIZER_LEN);
      r = strlen(sexpr());
      break;  
    case TOKENIZER_VAL:
     accept(TOKENIZER_VAL);
     r = atoi(sexpr());
	 break;
   case TOKENIZER_ASC:
    accept(TOKENIZER_ASC);
	s = sexpr();
	r = *s; 
	break;
   case TOKENIZER_INSTR:
    accept(TOKENIZER_INSTR);
	accept(TOKENIZER_LEFTPAREN);
	j = 1;
	if (tokenizer_token() == TOKENIZER_NUMBER) {
	  j = tokenizer_num();
	  accept(TOKENIZER_NUMBER);
	  accept(TOKENIZER_COMMA);
	} 
	if (j <1)
	   return 0;
	s = sexpr();
	accept(TOKENIZER_COMMA);
	s1 = sexpr();
	accept(TOKENIZER_RIGHTPAREN);
	r = sinstr(j, s, s1);
	break;	
 // end of string additions 
	 
  case TOKENIZER_NUMBER:
    r = tokenizer_num();
    DEBUG_PRINTF("factor: number %d.\n", r);
    accept(TOKENIZER_NUMBER);
    break;
  case TOKENIZER_LEFTPAREN:
    accept(TOKENIZER_LEFTPAREN);
    r = expr();
    accept(TOKENIZER_RIGHTPAREN);
    break;
  default:
    r = varfactor();
    break;
  }
  DEBUG_PRINTF("term: %d.\n", r);
  return r;
}
/*---------------------------------------------------------------------------*/
static int term(void) {
  int f1, f2;
  int op;
  if (tokenizer_stringlookahead()) {
    f1 = slogexpr();
  } else {
   f1 = factor();
   op = tokenizer_token();
   DEBUG_PRINTF("term: token '%s'.\n", tokenizer_token_name(op));
   while(op == TOKENIZER_ASTR ||
     op == TOKENIZER_SLASH ||
     op == TOKENIZER_MOD) {
     tokenizer_next();
     f2 = factor();
     DEBUG_PRINTF("term: %d %d %d\n", f1, op, f2);
     switch(op) {
       case TOKENIZER_ASTR:
        f1 = f1 * f2;
        break;
       case TOKENIZER_SLASH:
        f1 = f1 / f2;
        break;
       case TOKENIZER_MOD:
        f1 = f1 % f2;
        break;
     }
     op = tokenizer_token();
   }	 
  }
  DEBUG_PRINTF("term: factor=%d.\n", f1);
  return f1;
}
/*---------------------------------------------------------------------------*/
static VARIABLE_TYPE expr(void){
  int t1, t2;
  int op;

  t1 = term();
  op = tokenizer_token();
  DEBUG_PRINTF("expr: token %s.\n", tokenizer_token_name(op));
  while(op == TOKENIZER_PLUS ||
       op == TOKENIZER_MINUS ||
       op == TOKENIZER_AND ||
       op == TOKENIZER_OR) {
    tokenizer_next();
    t2 = term();
    DEBUG_PRINTF("expr: %d %d %d.\n", t1, op, t2);
    switch(op) {
    case TOKENIZER_PLUS:
      t1 = t1 + t2;
      break;
    case TOKENIZER_MINUS:
      t1 = t1 - t2;
      break;
    case TOKENIZER_AND:
      t1 = t1 & t2;
      break;
    case TOKENIZER_OR:
      t1 = t1 | t2;
      break;
    }
    op = tokenizer_token();
  }
  DEBUG_PRINTF("expr: term=%d.\n", t1);
  return t1;
}
/*---------------------------------------------------------------------------*/
static int relation(void){
  int r1, r2;
  int op;

  r1 = expr();
  op = tokenizer_token();
  DEBUG_PRINTF("relation: token %d.\n", op);
  while(op == TOKENIZER_LT ||
       op == TOKENIZER_GT ||
       op == TOKENIZER_EQ) {
    tokenizer_next();
    r2 = expr();
    DEBUG_PRINTF("relation: %d %d %d.\n", r1, op, r2);
    switch(op) {
    case TOKENIZER_LT:
      r1 = r1 < r2;
      break;
    case TOKENIZER_GT:
      r1 = r1 > r2;
      break;
    case TOKENIZER_EQ:
      r1 = r1 == r2;
      break;
    }
    op = tokenizer_token();
  }
  DEBUG_PRINTF("relation: expr=%d.\n", r1);
  return r1;
}
/*---------------------------------------------------------------------------*/
static void index_free(void) {
  if(line_index_head != NULL) {
    line_index_current = line_index_head;
    do {
      DEBUG_PRINTF("index_free: Freeing index for line %d.\n", line_index_current->line_number);
      line_index_head = line_index_current;
      line_index_current = line_index_current->next;
      free(line_index_head);
    } while (line_index_current != NULL);
    line_index_head = NULL;
  }
}
/*---------------------------------------------------------------------------*/
static char const* index_find(int linenum) {
  struct line_index *lidx;
  lidx = line_index_head;

  #if DEBUG
  int step = 0;
  #endif

  while(lidx != NULL && lidx->line_number != linenum) {
    lidx = lidx->next;

    #if DEBUG
    	#if VERBOSE
      		if(lidx != NULL) {
        		DEBUG_PRINTF("index_find: Step %3d. Found index for line %d: %p.\n",
        		step,
        		lidx->line_number,
        		lidx->program_text_position - tokenizer_start());
      		}
      		step++;
    	#endif
    #endif
  }
  if(lidx != NULL && lidx->line_number == linenum) {
    #if DEBUG
    	#if VERBOSE
      		DEBUG_PRINTF("index_find: Returning index for line %d.\n", linenum);
    	#endif
    #endif
    return lidx->program_text_position;
  }
  DEBUG_PRINTF("index_find: Returning NULL.\n", linenum);
  return NULL;
}
/*---------------------------------------------------------------------------*/
static void index_add(int linenum, char const* sourcepos) {
  if(line_index_head != NULL && index_find(linenum)) {
    return;
  }

  struct line_index *new_lidx;

  new_lidx = malloc(sizeof(struct line_index));
  new_lidx->line_number = linenum;
  new_lidx->program_text_position = sourcepos;
  new_lidx->next = NULL;

  if(line_index_head != NULL) {
    line_index_current->next = new_lidx;
    line_index_current = line_index_current->next;
  } else {
    line_index_current = new_lidx;
    line_index_head = line_index_current;
  }
  #if DEBUG
  	#if VERBOSE
		DEBUG_PRINTF("index_add: Adding index for line %d: %p.\n", linenum,
			sourcepos - tokenizer_start());
		#endif
	#endif
}
/*---------------------------------------------------------------------------*/
static void jump_linenum_slow(int linenum) {
  tokenizer_init(program_ptr);
  while(tokenizer_num() != linenum) {
    do {
      do {
        tokenizer_next();
      } while(tokenizer_token() != TOKENIZER_LF &&
          tokenizer_token() != TOKENIZER_ENDOFINPUT);
      if(tokenizer_token() == TOKENIZER_LF) {
        tokenizer_next();
      }
    } while(tokenizer_token() != TOKENIZER_NUMBER);
	#if DEBUG
      #if VERBOSE
        DEBUG_PRINTF("jump_linenum_slow: Found line %d.\n", tokenizer_num());
	  #endif
	#endif
  }
}
/*---------------------------------------------------------------------------*/
static void jump_linenum(int linenum) {
  char const* pos = index_find(linenum);
  if(pos != NULL) {
    DEBUG_PRINTF("jump_linenum: Going to line %d.\n", linenum);
    tokenizer_goto(pos);
  } else {
    /* We'll try to find a yet-unindexed line to jump to. */
    DEBUG_PRINTF("jump_linenum: Calling jump_linenum_slow for line %d.\n", linenum);
    jump_linenum_slow(linenum);
  }
}
/*---------------------------------------------------------------------------*/
static void goto_statement(void) {
  accept(TOKENIZER_GOTO);
  DEBUG_PRINTF("jump_linenum: goto.\n");
  jump_linenum(tokenizer_num());
}
/*---------------------------------------------------------------------------*/
static void print_statement(void) {
  // string additions
  static char buf[128];
  buf[0]=0;
  // end of string additions
  accept(TOKENIZER_PRINT);
  DEBUG_PRINTF("print_statement: Loop.\n");
  do {
    if(tokenizer_token() == TOKENIZER_STRING) {
      tokenizer_string(string, sizeof(string));
	  sprintf(buf+strlen(buf), "%s", string);
      tokenizer_next();
    } else if(tokenizer_token() == TOKENIZER_COMMA) {
      sprintf(buf+strlen(buf), "%s", " ");
      tokenizer_next();
    } else if(tokenizer_token() == TOKENIZER_SEMICOLON) {
      tokenizer_next();
    } else if(tokenizer_token() == TOKENIZER_VARIABLE ||
      tokenizer_token() == TOKENIZER_NUMBER) {
      sprintf(buf+strlen(buf), "%d", expr());
	} else if (tokenizer_token() == TOKENIZER_CR){
		tokenizer_next();
    } else {
	  // string additions
      if (tokenizer_stringlookahead()) {
          sprintf(buf+strlen(buf), "%s", sexpr());
      } else {
         sprintf(buf+strlen(buf), "%d", expr());
	  }
	  // end of string additions
	  break;
	  
    }
  } while(tokenizer_token() != TOKENIZER_LF &&
	  tokenizer_token() != TOKENIZER_ENDOFINPUT);
  printf(buf);
  printf("\n");
  DEBUG_PRINTF("print_statement: End of print.\n");
  tokenizer_next();
}
/*---------------------------------------------------------------------------*/
static void if_statement(void){
  int r;

  accept(TOKENIZER_IF);

  r = relation();
  #if VERBOSE
  	DEBUG_PRINTF("if_statement: relation %d.\n", r);
  #endif
  accept(TOKENIZER_THEN);
  if(r) {
    statement();
  } else {
    do {
      tokenizer_next();
    } while(tokenizer_token() != TOKENIZER_ELSE &&
        tokenizer_token() != TOKENIZER_LF &&
        tokenizer_token() != TOKENIZER_ENDOFINPUT);
    if(tokenizer_token() == TOKENIZER_ELSE) {
      tokenizer_next();
      statement();
    } else if(tokenizer_token() == TOKENIZER_LF) {
      tokenizer_next();
    }
  }
}
/*---------------------------------------------------------------------------*/
static void let_statement(void){
// string additions here
  int var;
  if (tokenizer_token() == TOKENIZER_VARIABLE) {
     var = tokenizer_variable_num();
     accept(TOKENIZER_VARIABLE);
     accept(TOKENIZER_EQ);
     set_variable(var, expr());
	 #if VERBOSE
    DEBUG_PRINTF("let_statement: assign %d to %d.\n", get_variable(var), var);
     #endif
	 accept(TOKENIZER_LF);
  } else if (tokenizer_token() == TOKENIZER_STRINGVARIABLE) {
     var = tokenizer_variable_num();
	 accept(TOKENIZER_STRINGVARIABLE);
     accept(TOKENIZER_EQ);
	 set_stringvariable(var, sexpr());
	 DEBUG_PRINTF("let_statement: string assign '%s' to %d\n", get_stringvariable(var), var);
	 accept(TOKENIZER_LF);

  }
  // end of string additions
}
/*---------------------------------------------------------------------------*/
static void gosub_statement(void) {
  int linenum;
  accept(TOKENIZER_GOSUB);
  linenum = tokenizer_num();
  accept(TOKENIZER_NUMBER);
  accept(TOKENIZER_LF);
  if(*gosub_depth_cell < UBASIC_MAX_GOSUB_STACK_DEPTH) {

    /* Push the current return line into the configured gosub stack (memory or internal). */
    gosub_push(tokenizer_num());

    jump_linenum(linenum);
  } else {
    DEBUG_PRINTF("gosub_statement: gosub stack exhausted.\n");
  }
}
/*---------------------------------------------------------------------------*/
static void return_statement(void){
  accept(TOKENIZER_RETURN);
  if(*gosub_depth_cell > 0) {
    int32_t retline = gosub_pop();
    jump_linenum(retline);
  } else {
    DEBUG_PRINTF("return_statement: non-matching return.\n");
  }
}
/*---------------------------------------------------------------------------*/
static void rem_statement(void) {
  accept(TOKENIZER_REM);
  DEBUG_PRINTF("rem_statement: Skipping comment.\n");
  tokenizer_skip();
  if (tokenizer_token() == TOKENIZER_LF) {
    accept(TOKENIZER_LF);
  }
}
/*---------------------------------------------------------------------------*/
static void next_statement(void){
  int var;

  accept(TOKENIZER_NEXT);
  var = tokenizer_variable_num();
  accept(TOKENIZER_VARIABLE);
  if(*for_depth_cell > 0 &&
     var == (int)for_stack[*for_depth_cell - 1].for_variable_index) {
    for_state top = for_stack[*for_depth_cell - 1];
    set_variable(var, get_variable(var) + 1);
    if(get_variable(var) <= top.to) {
      jump_linenum(top.line_after_for);
    } else {
      for_pop();
      accept(TOKENIZER_LF);
    }
  } else {
    if (*for_depth_cell > 0) {
      DEBUG_PRINTF("next_statement: non-matching next (expected %d, found %d).\n",
          (int)for_stack[*for_depth_cell - 1].for_variable_index, var);
    }
    accept(TOKENIZER_LF);
  }

}
/*---------------------------------------------------------------------------*/
static void for_statement(void) {
  int for_variable, to;

  accept(TOKENIZER_FOR);
  for_variable = tokenizer_variable_num();
  accept(TOKENIZER_VARIABLE);
  accept(TOKENIZER_EQ);
  set_variable(for_variable, expr());
  accept(TOKENIZER_TO);
  to = expr();
  accept(TOKENIZER_LF);

  for_state st;
  st.line_after_for = (int32_t)tokenizer_num();
  st.for_variable_index = (int32_t)for_variable;
  st.to = (int32_t)to;
  for_push(st);
  #if VERBOSE
    DEBUG_PRINTF("for_statement: new for, var %d to %d.\n", (int)st.for_variable_index, (int)st.to);
  #endif
  
}
/*---------------------------------------------------------------------------*/
static void peek_statement(void){
  VARIABLE_TYPE peek_addr;
  int var;

  accept(TOKENIZER_PEEK);
  peek_addr = expr();
  accept(TOKENIZER_COMMA);
  var = tokenizer_variable_num();
  accept(TOKENIZER_VARIABLE);
  accept(TOKENIZER_LF);

  set_variable(var, peek_function(peek_addr));
}
/*---------------------------------------------------------------------------*/
static void poke_statement(void)
{
  VARIABLE_TYPE poke_addr;
  VARIABLE_TYPE value;

  accept(TOKENIZER_POKE);
  poke_addr = expr();
  accept(TOKENIZER_COMMA);
  value = expr();
  accept(TOKENIZER_LF);

  poke_function(poke_addr, value);
}
/*---------------------------------------------------------------------------*/
static void end_statement(void)
{
  accept(TOKENIZER_END);
  DEBUG_PRINTF("end_statement: Ending program.\n");
  ended = 1;
}
/*---------------------------------------------------------------------------*/
static void statement(void){
  int token;

  token = tokenizer_token();

  switch(token) {
  case TOKENIZER_PRINT:
    print_statement();
    break;
  case TOKENIZER_IF:
    if_statement();
    break;
  case TOKENIZER_GOTO:
    goto_statement();
    break;
  case TOKENIZER_GOSUB:
    gosub_statement();
    break;
  case TOKENIZER_RETURN:
    return_statement();
    break;
  case TOKENIZER_FOR:
    for_statement();
    break;
  case TOKENIZER_PEEK:
    peek_statement();
    break;
  case TOKENIZER_POKE:
    poke_statement();
    break;
  case TOKENIZER_NEXT:
    next_statement();
    break;
  case TOKENIZER_END:
    end_statement();
    break;
  case TOKENIZER_LET:
    accept(TOKENIZER_LET);
    /* Fall through. */
  case TOKENIZER_VARIABLE:
  // string addition
  case TOKENIZER_STRINGVARIABLE:
  // end of string addition
    let_statement();
    break;
  case TOKENIZER_REM:
    rem_statement();
    break;
  default:
    DEBUG_PRINTF("statement: not implemented %d.\n", token);
    exit(1);
  }
}
/*---------------------------------------------------------------------------*/
static void line_statement(void){
  #if VERBOSE
    DEBUG_PRINTF("----------- Line number %d ---------\n", tokenizer_num());
  #endif
  index_add(tokenizer_num(), tokenizer_pos());
  accept(TOKENIZER_NUMBER);
  statement();
}
/*---------------------------------------------------------------------------*/
static void set_variable(int varnum, VARIABLE_TYPE value){
  if(variables_mem != NULL && varnum >= 0 && varnum < UBASIC_VARIABLE_COUNT) {
    variables_mem[varnum] = value;
  }
}
/*---------------------------------------------------------------------------*/
static VARIABLE_TYPE get_variable(int varnum){
  if(variables_mem != NULL && varnum >= 0 && varnum < UBASIC_VARIABLE_COUNT) {
    return variables_mem[varnum];
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
// string additions
/*---------------------------------------------------------------------------*/
void set_stringvariable(int svarnum, char *svalue) {
  ubasic_string_state_t *h;
  size_t len;
  uint16_t need;
  uint16_t new_base;

  if (svalue == NULL || mem_base == NULL) {
    return;
  }
  if (svarnum < 0 || svarnum >= MAX_SVARNUM) {
    return;
  }
  h = str_state();
  if (h == NULL) {
    return;
  }
  
  len = strlen(svalue);
  if (len > (size_t)MAX_STRINGVARLEN) {
    len = (size_t)MAX_STRINGVARLEN;
  }
  if (len == 0) {
    h->var_off[svarnum] = 0;
    return;
  }
  
  need = (uint16_t)(len + 1);
  
  /* Check if we have space (growing downward from pool_base toward pool_min) */
  if (h->pool_base < h->pool_min + need) {
    DEBUG_PRINTF("set_stringvariable: Out of string memory (need %u bytes, have %u).\n",
                 (unsigned)need, (unsigned)(h->pool_base - h->pool_min));
    ended = 1;
    return;
  }
  
  /* Allocate downward */
  new_base = h->pool_base - need;
  memcpy(mem_base + new_base, svalue, len);
  mem_base[new_base + len] = '\0';
  h->var_off[svarnum] = new_base;  /* Store absolute offset */
  h->pool_base = new_base;
  
  #if VERBOSE
    DEBUG_PRINTF("set_stringvariable: Assigned '%s' to var %d at offset %u.\n", 
                 svalue, svarnum, (unsigned)new_base);
  #endif
}
/*---------------------------------------------------------------------------*/
char *get_stringvariable(int varnum) {
  ubasic_string_state_t *h;
  uint16_t o;
  size_t string_state_offset;

  if (varnum < 0 || varnum >= MAX_SVARNUM || mem_base == NULL) {
    return scpy(nullstring);
  }
  h = str_state();
  if (h == NULL) {
    return scpy(nullstring);
  }
  
  o = h->var_off[varnum];
  if (o == 0) {
    /* Return empty string at pool_base (position 0 of pool_data) */
    return (char *)mem_base;  /* Points to empty string at offset 0 */
  }
  
  /* Validate offset is within pool bounds */
  string_state_offset = mem_capacity_bytes - UBASIC_VARIABLES_SIZE - UBASIC_STRING_STATE_SIZE;
  if (o < h->pool_min || o >= string_state_offset) {
    DEBUG_PRINTF("get_stringvariable: Invalid offset %u for var %d.\n", (unsigned)o, varnum);
    return scpy(nullstring);
  }
  
  return (char *)(mem_base + o);
}
/*---------------------------------------------------------------------------*/
static int32_t gosub_pop(void) {
  int32_t ptr = *gosub_depth_cell;
  if (ptr == 0) {
    DEBUG_PRINTF("gosub_pop: underflow (ptr=0)\n");
    return 0;
  }
  ptr--;
  *gosub_depth_cell = ptr;
  return gosub_stack_mem[ptr];
}

/*---------------------------------------------------------------------------*/
// end of string additions
/*---------------------------------------------------------------------------*/
static void gosub_push(int32_t line_num) {
  int32_t ptr = *gosub_depth_cell;
  if (ptr >= UBASIC_MAX_GOSUB_STACK_DEPTH) {
    DEBUG_PRINTF("gosub_push: overflow (ptr=%d)\n", (int)ptr);
    return;
  }
  gosub_stack_mem[ptr] = line_num;
  *gosub_depth_cell = ptr + 1;
}
/*---------------------------------------------------------------------------*/
static void for_push(for_state state) {
  int32_t depth = *for_depth_cell;
  if (depth < UBASIC_MAX_FOR_STACK_DEPTH) {
    for_stack[depth++] = state;
    *for_depth_cell = depth;
  } else {
    DEBUG_PRINTF("for_push: for stack depth exceeded.\n");
  }
}
/*---------------------------------------------------------------------------*/
static for_state for_pop(void) {
  for_state error_state = {-1, '\0', 0};
  int32_t depth = *for_depth_cell;
  if (depth > 0) {
    for_state value = for_stack[--depth];
    *for_depth_cell = depth;
    return value;
  } else {
    DEBUG_PRINTF("for_pop: for stack underflow.\n");
    return error_state;
  }
}
/*---------------------------------------------------------------------------*/



