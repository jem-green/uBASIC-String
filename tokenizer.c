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

#include "tokenizer.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static char const *ptr, *nextptr, *startptr;

static char const *prog;

#define MAX_NUMLEN 5

struct keyword_token {
  char *keyword;
  int token;
};

static int current_token = TOKENIZER_ERROR;

static const struct keyword_token keywords[] = {

// new string-related statements and functions
  {"left$",                   TOKENIZER_LEFT$},
  {"right$",                  TOKENIZER_RIGHT$},
  {"mid$",                    TOKENIZER_MID$},
  {"str$",                    TOKENIZER_STR$},
  {"chr$",                    TOKENIZER_CHR$},
  {"val",                     TOKENIZER_VAL},
  {"len",                     TOKENIZER_LEN},
  {"instr",                   TOKENIZER_INSTR},
  {"asc",                     TOKENIZER_ASC},
// end of string additions
 
  {"let", TOKENIZER_LET},
  {"print", TOKENIZER_PRINT},
  {"if", TOKENIZER_IF},
  {"then", TOKENIZER_THEN},
  {"else", TOKENIZER_ELSE},
  {"for", TOKENIZER_FOR},
  {"to", TOKENIZER_TO},
  {"next", TOKENIZER_NEXT},
  {"goto", TOKENIZER_GOTO},
  {"gosub", TOKENIZER_GOSUB},
  {"return", TOKENIZER_RETURN},
  {"call", TOKENIZER_CALL},
  {"rem", TOKENIZER_REM},
  {"peek", TOKENIZER_PEEK},
  {"poke", TOKENIZER_POKE},
  {"end", TOKENIZER_END},
  {NULL, TOKENIZER_ERROR}
};

static const struct keyword_token tokens[] = {
	{"TOKENIZER_ERROR",TOKENIZER_ERROR},
	{"TOKENIZER_ENDOFINPUT",TOKENIZER_ENDOFINPUT},
	{"TOKENIZER_NUMBER",TOKENIZER_NUMBER},
	{"TOKENIZER_STRING",TOKENIZER_STRING},
	{"TOKENIZER_VARIABLE",TOKENIZER_VARIABLE},
	{"TOKENIZER_STRINGVARIABLE",TOKENIZER_STRINGVARIABLE},
  	{"TOKENIZER_PRINT$", TOKENIZER_PRINT$},
  	{"TOKENIZER_LEFT$",TOKENIZER_LEFT$},
    {"TOKENIZER_RIGHT$",TOKENIZER_RIGHT$},
    {"TOKENIZER_MID$",TOKENIZER_MID$},
    {"TOKENIZER_STR$",TOKENIZER_STR$},
    {"TOKENIZER_CHR$",TOKENIZER_CHR$},
    {"TOKENIZER_VAL",TOKENIZER_VAL},
    {"TOKENIZER_LEN",TOKENIZER_LEN},
    {"TOKENIZER_INSTR",TOKENIZER_INSTR},
    {"TOKENIZER_ASC",TOKENIZER_ASC},
	{"TOKENIZER_LET",TOKENIZER_LET},
	{"TOKENIZER_PRINT",TOKENIZER_PRINT},
	{"TOKENIZER_IF",TOKENIZER_IF},
	{"TOKENIZER_THEN",TOKENIZER_THEN},
	{"TOKENIZER_ELSE",TOKENIZER_ELSE},
	{"TOKENIZER_FOR",TOKENIZER_FOR},
	{"TOKENIZER_TO",TOKENIZER_TO},
	{"TOKENIZER_NEXT",TOKENIZER_NEXT},
	{"TOKENIZER_GOTO",TOKENIZER_GOTO},
	{"TOKENIZER_GOSUB",TOKENIZER_GOSUB},
	{"TOKENIZER_RETURN",TOKENIZER_RETURN},
	{"TOKENIZER_CALL",TOKENIZER_CALL},
	{"TOKENIZER_REM",TOKENIZER_REM},
	{"TOKENIZER_PEEK",TOKENIZER_PEEK},
	{"TOKENIZER_POKE",TOKENIZER_POKE},
	{"TOKENIZER_END",TOKENIZER_END},
	{"TOKENIZER_COMMA",TOKENIZER_COMMA},
	{"TOKENIZER_SEMICOLON",TOKENIZER_SEMICOLON},
	{"TOKENIZER_PLUS",TOKENIZER_PLUS},
	{"TOKENIZER_MINUS",TOKENIZER_MINUS},
	{"TOKENIZER_AND",TOKENIZER_AND},
	{"TOKENIZER_OR",TOKENIZER_OR},
	{"TOKENIZER_ASTR",TOKENIZER_ASTR},
	{"TOKENIZER_SLASH",TOKENIZER_SLASH},
	{"TOKENIZER_MOD",TOKENIZER_MOD},
	{"TOKENIZER_HASH",TOKENIZER_HASH},
	{"TOKENIZER_LEFTPAREN",TOKENIZER_LEFTPAREN},
	{"TOKENIZER_RIGHTPAREN",TOKENIZER_RIGHTPAREN},
	{"TOKENIZER_LT",TOKENIZER_LT},
	{"TOKENIZER_GT",TOKENIZER_GT},
	{"TOKENIZER_EQ",TOKENIZER_EQ},
	{"TOKENIZER_LF",TOKENIZER_LF},
	{"TOKENIZER_CR",TOKENIZER_CR}
};

/*---------------------------------------------------------------------------*/
static int singlechar(void){
  if(*ptr == '\n') {
    return TOKENIZER_LF;
  } else if(*ptr == ',') {
    return TOKENIZER_COMMA;
  } else if(*ptr == ';') {
    return TOKENIZER_SEMICOLON;
  } else if(*ptr == '+') {
    return TOKENIZER_PLUS;
  } else if(*ptr == '-') {
    return TOKENIZER_MINUS;
  } else if(*ptr == '&') {
    return TOKENIZER_AND;
  } else if(*ptr == '|') {
    return TOKENIZER_OR;
  } else if(*ptr == '*') {
    return TOKENIZER_ASTR;
  } else if(*ptr == '/') {
    return TOKENIZER_SLASH;
  } else if(*ptr == '%') {
    return TOKENIZER_MOD;
  } else if(*ptr == '(') {
    return TOKENIZER_LEFTPAREN;
  } else if(*ptr == '#') {
    return TOKENIZER_HASH;
  } else if(*ptr == ')') {
    return TOKENIZER_RIGHTPAREN;
  } else if(*ptr == '<') {
    return TOKENIZER_LT;
  } else if(*ptr == '>') {
    return TOKENIZER_GT;
  } else if(*ptr == '=') {
    return TOKENIZER_EQ;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
static int get_next_token(void) {
  struct keyword_token const *kt;
  int i;

  DEBUG_PRINTF("get_next_token: %p.\n", ptr-startptr);
  
  // eat all whitespace
  while(*ptr == ' ' || *ptr == '\t' || *ptr == '\r')
    ptr++;

  if(*ptr == 0) {
    return TOKENIZER_ENDOFINPUT;
  }

  if(isdigit(*ptr)) {
    for(i = 0; i < MAX_NUMLEN; ++i) {
      if(!isdigit(ptr[i])) {
        if(i > 0) {
          nextptr = ptr + i;
          return TOKENIZER_NUMBER;
        } else {
          DEBUG_PRINTF("get_next_token: error due to too short number.\n");
          return TOKENIZER_ERROR;
        }
      }
      if(!isdigit(ptr[i])) {
        DEBUG_PRINTF("get_next_token: error due to malformed number.\n");
        return TOKENIZER_ERROR;
      }
    }
    DEBUG_PRINTF("get_next_token: error due to too long number.\n");
    return TOKENIZER_ERROR;
  } else if(singlechar()) {
    nextptr = ptr + 1;
    return singlechar();
  } else if(*ptr == '"') {
    nextptr = ptr;
    do {
      ++nextptr;
    } while(*nextptr != '"');
    ++nextptr;
    return TOKENIZER_STRING;
  } else {
    for(kt = keywords; kt->keyword != NULL; ++kt) {
      if(strncmp(ptr, kt->keyword, strlen(kt->keyword)) == 0) {
        nextptr = ptr + strlen(kt->keyword);
        return kt->token;
      }
    }
  }

  if(*ptr >= 'a' && *ptr <= 'z') {

// string addition
    if (*(ptr+1) == '$') {
       nextptr = ptr + 2;
	   return TOKENIZER_STRINGVARIABLE;
	}
// end of string addition

    nextptr = ptr + 1;
    return TOKENIZER_VARIABLE;
  }

  return TOKENIZER_ERROR;
}

/*---------------------------------------------------------------------------*/
int tokenizer_stringlookahead(void) { 
  // return 1 (true) if next 'defining' token is string not integer
  char const *saveptr = ptr;
  char const *savenextptr = nextptr;
  int token = current_token;
  int si = -1;
  
  while (si == -1) {
     if (token == TOKENIZER_LF || token == TOKENIZER_ENDOFINPUT)
	    si = 0;
	 else if (token == TOKENIZER_NUMBER || token == TOKENIZER_VARIABLE)
	    si = 0; // number or numeric var
	 else if (token == TOKENIZER_PLUS)
	    si = si;
	 else if (token == TOKENIZER_STRING)
	    si = 1;
	 else if (token >= TOKENIZER_STRINGVARIABLE && token <= TOKENIZER_CHR$)
	    si = 1;
	 else if (token > TOKENIZER_CHR$)
	    si = 0; // numeric function
     token = get_next_token();   
  }
  ptr = saveptr;
  nextptr = savenextptr;
  return si; 
}
/*---------------------------------------------------------------------------*/
void tokenizer_goto(const char *program){
  ptr = program;
  current_token = get_next_token();
}
/*---------------------------------------------------------------------------*/
void tokenizer_init(const char *program){
  ptr = program;
  prog = program;
  startptr = program;
  current_token = get_next_token();
}
/*---------------------------------------------------------------------------*/
int tokenizer_token(void){
  return current_token;
}
/*---------------------------------------------------------------------------*/
void tokenizer_next(void){

  if(tokenizer_finished()) {
    return;
  }

  DEBUG_PRINTF("tokenizer_next: %p.\n", nextptr - startptr);
  ptr = nextptr;

  while(*ptr == ' ') {
    ++ptr;
  }
  current_token = get_next_token();

  DEBUG_PRINTF("tokenizer_next: %p %s.\n", ptr-startptr, tokenizer_token_name(current_token));
}

void tokeniser_skip(void) {
  while(!(*nextptr == '\n' || tokenizer_finished())) {
        ++nextptr;
  }
  if (*nextptr == '\n') {
    nextptr++;
  }
  ptr = nextptr;
}

/*---------------------------------------------------------------------------*/
VARIABLE_TYPE tokenizer_num(void)
{
  return atoi(ptr);
}
/*---------------------------------------------------------------------------*/
void tokenizer_string(char *dest, int len){
  char *string_end;
  int string_len;

  if(tokenizer_token() != TOKENIZER_STRING) {
    return;
  }
  string_end = strchr(ptr + 1, '"');
  if(string_end == NULL) {
    return;
  }
  string_len = string_end - ptr - 1;
  if(len < string_len) {
    string_len = len;
  }
  memcpy(dest, ptr + 1, string_len);
  dest[string_len] = 0;
}
/*---------------------------------------------------------------------------*/
void tokenizer_error_print(void) {
  DEBUG_PRINTF("tokenizer_error_print: %p.\n", ptr-startptr);
}

/*---------------------------------------------------------------------------*/
int tokenizer_finished(void) {
  return *ptr == 0 || current_token == TOKENIZER_ENDOFINPUT;
}

/*---------------------------------------------------------------------------*/
int tokenizer_variable_num(void) {
  return *ptr - 'a';
}
/*---------------------------------------------------------------------------*/
char const *tokenizer_pos(void){
    return ptr;
}

/*---------------------------------------------------------------------------*/
char const *tokenizer_start(void) {
	return startptr;
}

/*---------------------------------------------------------------------------*/
char *tokenizer_token_name(int token) {
    for (int i = 0; tokens[i].keyword != NULL; i++) {
        if (tokens[i].token == token) {
            return tokens[i].keyword;  // return string
        }
    }
    return NULL; // not found
}