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
#include <string.h> /* strlen() etc */

static char const *program_ptr;
#define MAX_STRINGLEN 40
static char string[MAX_STRINGLEN];

// string additions
#define MAX_STRINGVARLEN 255
#define MAX_BUFFERLEN    4000
#define GBGCHECK         3500
static char stringbuffer[MAX_BUFFERLEN];
static int freebufptr = 0;
#define MAX_SVARNUM 26 
static char *stringvariables[MAX_SVARNUM];
// end of string additions

#define MAX_GOSUB_STACK_DEPTH 10
static int gosub_stack[MAX_GOSUB_STACK_DEPTH];
static int gosub_stack_ptr;

struct for_state {
  int line_after_for;
  int for_variable;
  int to;
};
#define MAX_FOR_STACK_DEPTH 4
static struct for_state for_stack[MAX_FOR_STACK_DEPTH];
static int for_stack_ptr;

struct line_index {
  int line_number;
  char const *program_text_position;
  struct line_index *next;
};
struct line_index *line_index_head = NULL;
struct line_index *line_index_current = NULL;
#define MAX_VARNUM 26
static VARIABLE_TYPE variables[MAX_VARNUM];

static int ended;

peek_func peek_function = NULL;
poke_func poke_function = NULL;

// string additions
static char nullstring[] = "\0"; 
// end of string addition

/*---------------------------------------------------------------------------*/
void ubasic_init(const char *program){
  program_ptr = program;
  for_stack_ptr = gosub_stack_ptr = 0;
  index_free();
  tokenizer_init(program);
  var_init(); // string addition
  ended = 0;
}
/*---------------------------------------------------------------------------*/
void ubasic_init_peek_poke(const char *program, peek_func peek, poke_func poke){
  program_ptr = program;
  for_stack_ptr = gosub_stack_ptr = 0;
  index_free();
  peek_function = peek;
  poke_function = poke;
  tokenizer_init(program);
  ended = 0;
}
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

// string additions
/*---------------------------------------------------------------------------*/
static void var_init() {
   int i;
   freebufptr = 0;
   for (i=0; i<MAX_VARNUM; i++) 
      variables[i] = 0;
   for (i=0; i<MAX_SVARNUM; i++) 
	  stringvariables[i] = scpy(nullstring);
}
/*---------------------------------------------------------------------------*/
int string_space_check(int l) {
   // returns true if not enough room for new string
   int i;
   i = ((MAX_BUFFERLEN - freebufptr) <= (l + 2)); // +2 to play it safe
   if (i) {
      ended = 1;
   }
   return i;
}
/*---------------------------------------------------------------------------*/
void garbage_collect() {
  int totused = 0;
  int i;
  char *temp;
  char *tp;
  if (freebufptr < GBGCHECK)
     return;
  for (i=0; i< MAX_SVARNUM; i++) { // calculate used space
     totused += strlen(stringvariables[i]) + 1;
  }
  DEBUG_PRINTF("Garbage collector called - reclaiming %d bytes\n", (freebufptr - totused));
  temp = malloc(totused);	// alloc temporary space to store vars
  tp = temp;
  for (i=0; i< MAX_SVARNUM; i++) { // copy used strings to temporary store
     strcpy(tp, stringvariables[i]);
	 tp += strlen(tp) + 1;
  }
  freebufptr = 0;
  tp = temp;
  for (i=0; i< MAX_SVARNUM; i++) { //copy back to buffer
     stringvariables[i] = scpy(tp);
	 tp+= strlen(tp) + 1;
  }
  
  free(temp); // free temp space
 }
/*---------------------------------------------------------------------------*/
static char* scpy(char *s1) { // return a copy of s1
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
static char* sconcat(char *s1, char*s2) { // return the concatenation of s1 and s2
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
static char* sleft(char *s1, int l) { // return the left l chars of s1
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
static char* sright(char *s1, int l) { // return the right l chars of s1
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
static char* smid(char *s1, int l1, int l2) { // return the l2 chars of s1 starting at offset l1
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
   freebufptr = bp + 1;
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
static int sinstr(int j, char *s, char *s1) { // return the position of s1 in s (or 0) 
   char *p;
   p = strstr(s+j, s1);
   if (p == NULL)
      return 0;
   return (p - s + 1);
}
/*---------------------------------------------------------------------------*/
char* sfactor() { // string form of factor
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
		r = ubasic_get_stringvariable(tokenizer_variable_num());
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
// end of string additions

/*---------------------------------------------------------------------------*/
static int varfactor(void)
{
  int r;
  DEBUG_PRINTF("varfactor: obtaining %d from variable %d.\n", variables[tokenizer_variable_num()], tokenizer_variable_num());
  r = ubasic_get_variable(tokenizer_variable_num());
  accept(TOKENIZER_VARIABLE);
  return r;
}
/*---------------------------------------------------------------------------*/
static int factor(void){
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
static int term(void){
  int f1, f2;
  int op;
  if (tokenizer_stringlookahead()) {
    f1 = slogexpr();
  } else {
    f1 = factor();
    op = tokenizer_token();
    DEBUG_PRINTF("term: token %d\n", op);
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
      DEBUG_PRINTF("Freeing index for line %d.\n", line_index_current->line_number);
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
static void jump_linenum_slow(int linenum)
{
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
static void jump_linenum(int linenum)
{
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
static void goto_statement(void)
{
  accept(TOKENIZER_GOTO);
  jump_linenum(tokenizer_num());
}
/*---------------------------------------------------------------------------*/
static void print_statement(void) {
// string additions
  static char buf[128];
  buf[0]=0;

  accept(TOKENIZER_PRINT);
  DEBUG_PRINTF("print_statement: Loop.\n");
  do {
    if(tokenizer_token() == TOKENIZER_STRING) {
      tokenizer_string(string, sizeof(string));
      printf("%s", string);
      tokenizer_next();
    } else if(tokenizer_token() == TOKENIZER_COMMA) {
      printf(" ");
      tokenizer_next();
    } else if(tokenizer_token() == TOKENIZER_SEMICOLON) {
      tokenizer_next();
    } else if(tokenizer_token() == TOKENIZER_VARIABLE ||
          tokenizer_token() == TOKENIZER_NUMBER) {
      printf("%d", expr());
	} else if (tokenizer_token() == TOKENIZER_CR){
		tokenizer_next();
    } else {
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
  DEBUG_PRINTF("if_statement: relation %d.\n", r);
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

  int var;
  if (tokenizer_token() == TOKENIZER_VARIABLE) {
     var = tokenizer_variable_num();
     accept(TOKENIZER_VARIABLE);
     accept(TOKENIZER_EQ);
     ubasic_set_variable(var, expr());
     DEBUG_PRINTF("let_statement: assign %d to %d.\n", variables[var], var);
     accept(TOKENIZER_LF);
  // string additions here
  } else if (tokenizer_token() == TOKENIZER_STRINGVARIABLE) {
     var = tokenizer_variable_num();
	 accept(TOKENIZER_STRINGVARIABLE);
     accept(TOKENIZER_EQ);
	 ubasic_set_stringvariable(var, sexpr());
	 DEBUG_PRINTF("let_statement: string assign '%s' to %d\n", stringvariables[var], var);
	 accept(TOKENIZER_LF);

  }
  // end of string additions
}
/*---------------------------------------------------------------------------*/
static void
gosub_statement(void){
  int linenum;
  accept(TOKENIZER_GOSUB);
  linenum = tokenizer_num();
  accept(TOKENIZER_NUMBER);
  accept(TOKENIZER_LF);
  if(gosub_stack_ptr < MAX_GOSUB_STACK_DEPTH) {
    gosub_stack[gosub_stack_ptr] = tokenizer_num();
    gosub_stack_ptr++;
    jump_linenum(linenum);
  } else {
    DEBUG_PRINTF("gosub_statement: gosub stack exhausted.\n");
  }
}
/*---------------------------------------------------------------------------*/
static void return_statement(void){
  accept(TOKENIZER_RETURN);
  if(gosub_stack_ptr > 0) {
    gosub_stack_ptr--;
    jump_linenum(gosub_stack[gosub_stack_ptr]);
  } else {
    DEBUG_PRINTF("return_statement: non-matching return.\n");
  }
}
/*---------------------------------------------------------------------------*/
static void next_statement(void){
  int var;

  accept(TOKENIZER_NEXT);
  var = tokenizer_variable_num();
  accept(TOKENIZER_VARIABLE);
  if(for_stack_ptr > 0 &&
     var == for_stack[for_stack_ptr - 1].for_variable) {
        ubasic_set_variable(var,
        ubasic_get_variable(var) + 1);
    if(ubasic_get_variable(var) <= for_stack[for_stack_ptr - 1].to) {
      jump_linenum(for_stack[for_stack_ptr - 1].line_after_for);
    } else {
      for_stack_ptr--;
      accept(TOKENIZER_LF);
    }
  } else {
    DEBUG_PRINTF("next_statement: non-matching next (expected %d, found %d).\n", for_stack[for_stack_ptr - 1].for_variable, var);
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
  ubasic_set_variable(for_variable, expr());
  accept(TOKENIZER_TO);
  to = expr();
  accept(TOKENIZER_LF);

  if(for_stack_ptr < MAX_FOR_STACK_DEPTH) {
    for_stack[for_stack_ptr].line_after_for = tokenizer_num();
    for_stack[for_stack_ptr].for_variable = for_variable;
    for_stack[for_stack_ptr].to = to;
    DEBUG_PRINTF("for_statement: new for, var %d to %d.\n",
                for_stack[for_stack_ptr].for_variable,
                for_stack[for_stack_ptr].to);

    for_stack_ptr++;
  } else {
    DEBUG_PRINTF("for_statement: for stack depth exceeded.\n");
  }
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

  ubasic_set_variable(var, peek_function(peek_addr));
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
  default:
    DEBUG_PRINTF("statement: not implemented %d.\n", token);
    exit(1);
  }
}
/*---------------------------------------------------------------------------*/
static void line_statement(void){
  DEBUG_PRINTF("----------- Line number %d ---------\n", tokenizer_num());
  index_add(tokenizer_num(), tokenizer_pos());
  accept(TOKENIZER_NUMBER);
  statement();
  return;
}
/*---------------------------------------------------------------------------*/
void ubasic_run(void){
  if(tokenizer_finished()) {
    DEBUG_PRINTF("ubasic_run: Program finished.\n");
    return;
  }
  // string additions
  garbage_collect();
  // end of string additions

  line_statement();
}
/*---------------------------------------------------------------------------*/
int ubasic_finished(void){
  return ended || tokenizer_finished();
}
/*---------------------------------------------------------------------------*/
void ubasic_set_variable(int varnum, VARIABLE_TYPE value){
  if(varnum > 0 && varnum <= MAX_VARNUM) {
    variables[varnum] = value;
  }
}
/*---------------------------------------------------------------------------*/
VARIABLE_TYPE ubasic_get_variable(int varnum){
  if(varnum > 0 && varnum <= MAX_VARNUM) {
    return variables[varnum];
  }
  return 0;
}
// string additions
/*---------------------------------------------------------------------------*/
void ubasic_set_stringvariable(int svarnum, char *svalue) {

    if(svarnum >=0 && svarnum <MAX_SVARNUM) {
	   stringvariables[svarnum] = svalue;
  	}
}
/*---------------------------------------------------------------------------*/
char* ubasic_get_stringvariable(int varnum){
  if(varnum>=0 && varnum< MAX_SVARNUM) {
      return stringvariables[varnum];
  }
  return scpy(nullstring);
}
// end of string additions

/*---------------------------------------------------------------------------*/
