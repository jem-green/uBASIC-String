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
static int extract_linenum_and_content(const char *line, uint32_t *linenum, const char **content_start);
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

// Error logging for CLI: 2-char code and description
static void cli_error(const char *code, const char *description) {
  if (code && code[0] && code[1]) {
    fprintf(stderr, "%c%c %s\n", code[0], code[1], description ? description : "");
  } else {
    fprintf(stderr, "?? %s\n", description ? description : "");
  }
}

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


  printf("uBasic CLI - Commands: RUN, LIST, NEW, LOAD <file>, SAVE <file>, EXIT\n");

  while (1) {
    printf("> ");
    fgets(input, MAX_LINE_LENGTH, stdin);

    // Ignore empty input (just Enter)
    if (input[0] == 0) {
	    continue;
    } else if (strncmp(input, "RUN", 3) == 0) {
      run_program();
    } else if (strncmp(input, "LIST", 4) == 0) {
      // Consider checking to see if a linenumber is provided
      printf("%s", program);
    } else if (strncmp(input, "NEW", 3) == 0) {
      memset(program, 0, sizeof(program)); // Sets all elements to 0
    } else if (strncmp(input, "EXIT", 4) == 0) {
        break;
    } else if (strncmp(input, "LOAD", 4) == 0) {
      char *filename = input + 4;
      // Eat whitespace
      while (*filename == ' ' || *filename == '\t') filename++;
      DEBUG_PRINTF("filename: '%s'\n", filename);
      if (*filename == 0) {
        cli_error("SY", "Syntax: LOAD <filename>");
      } else {
        // Strip trailing \r and \n
        size_t flen = strlen(filename);
        while (flen > 0 && (filename[flen-1] == '\r' || filename[flen-1] == '\n')) {
          filename[--flen] = '\0';
        }
        // Remove surrounding quotes if present
        flen = strlen(filename);
        if (flen > 1 && filename[0] == '"' && filename[flen-1] == '"') {
          filename[flen-1] = '\0';
          filename++;
        }
        FILE *f = fopen(filename, "r");
        if (!f) {
          cli_error("IO", "Could not open file for reading");
        } else {
          size_t n = fread(program, 1, MAX_PROGRAM_SIZE-1, f);
          program[n] = '\0';
          fclose(f);
          printf("Loaded %zu bytes from %s\n", n, filename);
        }
      }
    } else if (strncmp(input, "SAVE", 4) == 0) {
      char *filename = input + 4;
      // Eat whitespace
      while (*filename == ' ' || *filename == '\t') filename++;
      DEBUG_PRINTF("filename: '%s'\n", filename);
      if (*filename == 0) {
        cli_error("SY", "Syntax: SAVE <filename>");
      } else {
        // Strip trailing \r and \n
        size_t flen = strlen(filename);
        while (flen > 0 && (filename[flen-1] == '\r' || filename[flen-1] == '\n')) {
          filename[--flen] = '\0';
        }
        // Remove surrounding quotes if present
        flen = strlen(filename);
        if (flen > 1 && filename[0] == '"' && filename[flen-1] == '"') {
          filename[flen-1] = '\0';
          filename++;
        }
        FILE *f = fopen(filename, "w");
        if (!f) {
          cli_error("IO", "Could not open file for writing");
        } else {
          size_t n = fwrite(program, 1, strlen(program), f);
          fclose(f);
          printf("Saved %zu bytes to %s\n", n, filename);
        }
      }
    } else {
        // Check if input is a valid line (starts with a number) or treat as invalid command
        uint32_t dummy;
        const char *dummy_content;
        if (extract_linenum_and_content(input, &dummy, &dummy_content)) {
            add_line(input);
        } else {
            cli_error("IC", "Unrecognized command");
        }
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
  uint32_t linenum = 0;
  const char *content = NULL;
  int has_linenum = extract_linenum_and_content(line, &linenum, &content);
  if (!has_linenum || linenum == 0) {
    DEBUG_PRINTF("add_line: No valid line number found\n");
    return;
  }

  // Check if content after line number is empty (delete request)
  int is_delete = 1;
  for (const char *p = content; *p != '\0'; ++p) {
    if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
      is_delete = 0;
      break;
    }
  }

  DEBUG_PRINTF("add_line: line %u, is_delete=%d\n", linenum, is_delete);

  uint32_t r = find_linenum(linenum);
  DEBUG_PRINTF("add_line: Found line number %u\n", r);
  if (linenum != r) {
    if (!is_delete) {
      DEBUG_PRINTF("add_line: Insert new line %u\n", linenum);
      insert_char_array(startptr, line, ptr);
    }
    // else: trying to delete a non-existent line, do nothing
  } else {
    DEBUG_PRINTF("add_line: replace/delete existing line %u\n", linenum);
    int numChars = get_current_line_len();
    DEBUG_PRINTF("add_line: numchars %d\n", numChars);
    delete_char_array(startptr, ptr, numChars);
    if (!is_delete) {
      insert_char_array(startptr, line, ptr);
    }
    // else: line deleted
  }
  DEBUG_PRINTF("add_line: exit\n");
}

// Helper: Extracts line number and pointer to content after it
// Returns 1 if a valid line number is found, 0 otherwise
static int extract_linenum_and_content(const char *line, uint32_t *linenum, const char **content_start) {
  const char *p = line;
  // Skip leading whitespace
  while (*p == ' ' || *p == '\t') ++p;
  // Parse number
  char numbuf[16];
  int i = 0;
  while (*p >= '0' && *p <= '9' && i < 15) {
    numbuf[i++] = *p++;
  }
  numbuf[i] = '\0';
  if (i == 0) return 0; // No number found
  *linenum = (uint32_t)strtoul(numbuf, NULL, 10);
  // Skip whitespace after number
  while (*p == ' ' || *p == '\t') ++p;
  *content_start = p;
  return 1;
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
    DEBUG_PRINTF("get_current_line_len: %Id\n", nextptr - ptr);
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



