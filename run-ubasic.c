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

#include "ubasic.h"
#include <stdio.h>
#include <errno.h>

/*---------------------------------------------------------------------------*/
// main routine modified to allow execution of BASIC script files 

int main(int argc, char **argv, char **envp)
{
  char *q;
  char **p;
  char *prog;
  char buffer[15000];
  int infile;

  if (argc > 1) {
     p = argv + 1;
     q = *p;
	 while(*q == ' ') ++q;
 
     if ((infile = open(q,0)) == -1) {
        printf("File \"%s\" not found in current directory - terminating\n",q);
        return (-1);
     }

	 int bytes = read(infile,buffer,15000);
     if (bytes < 0) {
        printf("Error reading file \"%s\"  - terminating\n",q);
        printf("Error was \"%d\" \n",errno);
        return (-1);
     }
	 buffer[bytes] = '\0';
     prog = buffer;
  } else {
    printf("Usage: ubasic fname\n  where fname is a file containing basic statements\n");
    return (0);
  }

  ubasic_init(prog);
  do {
    ubasic_run();
  } while(!ubasic_finished());

  return 0;
}