/*
 * Copyright (c) 2026, Jeremy Green
 */



#define DEBUG 0
#define VERBOSE 0

#if DEBUG
#define DEBUG_PRINTF(...)  printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif

#include <stdio.h>
#include <string.h>
#include "ubasic.h"
#include "tokenizer.h"

// Private methods

static void run_program(void);
static void add_line(const char *line);
static void insert_char_array(char *dest, const char *src, char *position);
static int get_current_line_len(void);
static void delete_char_array(char *dest, char *position, int numChars);
static uint32_t find_linenum(uint32_t linenum);

#define MAX_LINE_LENGTH 255
#define MEMORY_SIZE 8192
#define MAX_PROGRAM_SIZE 4096

static char memory[MEMORY_SIZE];
static char program[MAX_PROGRAM_SIZE];

static char *ptr, *nextptr, *startptr;

/*---------------------------------------------------------------------------*/
int main(int argc, char* argv[]) {

  char *q;
  char **p;
  char buffer[MEMORY_SIZE];
  int infile;

  if (argc > 1) {
    p = argv + 1;
    q = *p;
    while(*q == ' ') ++q; // Eat whitespace

     if ((infile = open(q,0)) == -1) {
        printf("File \"%s\" not found in current directory - terminating\n",q);
        return (-1);
     }

    int bytes = read(infile,buffer,MEMORY_SIZE);
    if (bytes < 0) {
      printf("Error reading file \"%s\"  - terminating\n",q);
      printf("Error was \"%d\" \n",errno);
      return (-1);
    }
    buffer[bytes] = '\0';
    // Copy buffer to program
    if (bytes + 1 > MAX_PROGRAM_SIZE) {
      printf("Program file too large - terminating\n");
      return (-1);
    }
    memcpy(program, buffer, bytes + 1);

  }
  
  char input[MAX_LINE_LENGTH];

  printf("uBasic CLI - Type 'RUN' to execute, 'LIST' to view, 'NEW' to clear, 'EXIT' to quit\n");

  while (1) {
    printf("> ");
    fgets(input, MAX_LINE_LENGTH, stdin);

    if (strncmp(input, "RUN", 3) == 0) {
      run_program();
    } else if (strncmp(input, "LIST", 4) == 0) {
      // Consider checking to see if a linenumber is provided
      printf("%s", program);
    } else if (strncmp(input, "NEW", 3) == 0) {
      memset(program, 0, sizeof(program)); // Sets all elements to 0
    } else if (strncmp(input, "EXIT", 4) == 0) {
        break;
    } else {
        add_line(input);
    }
  }

  return 0;
}
/*---------------------------------------------------------------------------*/
static void run_program(void) {
    ubasic_init(memory, MEMORY_SIZE);
    ubasic_load_program(program);
    while (!ubasic_finished()) {
        ubasic_run();
    }
}
/*---------------------------------------------------------------------------*/
static void add_line(const char *line) {

  startptr = program;
  uint32_t linenum;
  static char const *lineptr;
  lineptr = line;
  linenum = (uint32_t)strtoul(lineptr, NULL, 10);
  int linelen = strlen(line);
  
  // want to check if the new line has content
  
  if (linenum > 0) {
    DEBUG_PRINTF("add_line: Add inputted line number %u\n", linenum);
    // Problem with find_linenumber of there are no line number
    // ptr returns pointing to the end of the previous lineno being
    // search for.
    uint32_t r = find_linenum(linenum);
    DEBUG_PRINTF("add_line: Found line number %u\n", r);
    if (linenum != r) {
      DEBUG_PRINTF("add_line: Insert new line %u\n",linenum);
      insert_char_array(startptr, line, ptr);
    } else {
      DEBUG_PRINTF("add_line: replace existing line %u\n",linenum);
      int numChars = 0;
      numChars = get_current_line_len();
      DEBUG_PRINTF("add_line: numchars %d\n",numChars);
      delete_char_array(startptr, ptr, numChars);
      
      // Need a better way to check if the line is just the line number
      
      if (linelen > 4) {
        insert_char_array(startptr, line, ptr);
      }
    }
  }
  
  DEBUG_PRINTF("add_line: exit\n");
  
}

/*---------------------------------------------------------------------------*/
static void insert_char_array(char *dest, const char *src, char *position) {
  int destLen = strlen(dest);
  int srcLen = strlen(src);

  // Ensure the position pointer is within the bounds of the destination string
  if (position < dest || position > dest + destLen) {
      DEBUG_PRINTF("insert_char_array: Invalid position pointer\n");
      return;
  }

  // Calculate the offset (index) from the start of `dest` to the position pointer
  int offset = position - dest;

  // Shift the contents of `dest` to make room for `src`
  memmove(dest + offset + srcLen, dest + offset, destLen - offset + 1);

  // Copy `src` into the desired position
  memcpy(dest + offset, src, srcLen);
}
/*---------------------------------------------------------------------------*/
static void delete_char_array(char *dest, char *position, int numChars) {
    int destLen = strlen(dest);

    // Ensure the start pointer is within bounds
    if (position < dest || position >= dest + destLen) {
        DEBUG_PRINTF("delete_char_array: Invalid start pointer\n");
        return;
    }

  // Calculate the offset (index) from the start of `dest` to the position pointer
    int offset = position - dest;

    // Ensure the number of characters to delete does not exceed the string length
    if (offset + numChars > destLen) {
        numChars = destLen - offset; // Adjust to delete only up to the end of the string
    }

    // Shift the remaining characters to the left
    memmove(position, position + numChars, destLen - offset - numChars + 1); // +1 to include the null terminator
}

/*---------------------------------------------------------------------------*/
static uint32_t find_linenum(uint32_t linenum) {
  #if DEBUG 
	#if VERBOSE
    DEBUG_PRINTF("find_linenum: enter.\n");
 	#endif
	#endif 
  uint32_t currentLinenum;
  ptr = program;
  currentLinenum = (uint32_t)strtoul(ptr, NULL, 10);
  while ((strtoul(ptr, NULL, 10) < linenum) && (*ptr != 0)) {
    #if DEBUG
    #if VERBOSE
      DEBUG_PRINTF("find_linenum: Current line %u.\n", (uint32_t)strtoul(ptr, NULL, 10));
    #endif
    #endif

    currentLinenum = (uint32_t)strtoul(ptr, NULL, 10);

    // Move to the end of the line
    // or end of the file

    while ((*ptr != '\n') && (*ptr != 0)) {
      ptr++;
    }
    ptr++;

    // Prevent replacing the last found linenum if end of file.
    if (strtoul(ptr, NULL, 10) > 0) {
      currentLinenum = (uint32_t)strtoul(ptr, NULL, 10);
    }
    #if DEBUG
    #if VERBOSE
      DEBUG_PRINTF("find_linenum: next Line %u.\n", currentLinenum);
    #endif
    #endif
  }
  DEBUG_PRINTF("find_linenum: Found line %u.\n", currentLinenum);
  return(currentLinenum);
}


/*---------------------------------------------------------------------------*/
static int get_current_line_len(void) {
  nextptr = ptr;
  do {
      nextptr++;
    } while (*nextptr != '\n');
  nextptr++;  
  #if DEBUG
	#if VERBOSE
    DEBUG_PRINTF("get_current_line_len: %d\n", nextptr - ptr);
	#endif
	#endif
  return (nextptr - ptr);
}





// Development
// Need to parse the input line to get the line number
// Perhaps Tokenize the input line
// Need to search for a line based on the input line number
// Need to be able to delete a line (current line)
// Need to move to the start of the next line
// Need to be able to insert a new line
// Need to be able to replace an exisitng line
//



