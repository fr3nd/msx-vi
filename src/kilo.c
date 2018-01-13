#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "asm.h"

Z80_registers regs;

#define _TERM0  0x00
#define _TERM   0x62
#define _DOSVER 0x6F
#define _INITXT 0x006C

#define perror(x) {printf("*** ");printf(x);printf("\r\n");}

/*** functions ***/

void die(const char *s) {
  perror(s);
  regs.Bytes.B = 1;
  DosCall(_TERM, &regs, REGS_NONE, REGS_NONE);
}

void init() {
  static char __at(0xF3AE) LINL40;

  // Check MSX-DOS version >= 2
  DosCall(_DOSVER, &regs, REGS_NONE, REGS_MAIN);
  if(regs.Bytes.B < 2) {
    die("This program requires MSX-DOS 2 to run.");
  }

  // Set 80 column mode
  LINL40 = 80;
  BiosCall(_INITXT, &regs, REGS_NONE);
}

void quit_program() {
}

/*** main ***/

int main() {
  char c;

  init();

  do {
    c = getchar();
    if (iscntrl(c)) {
      printf("%d\n\r", c);
    } else {
      printf("%d ('%c')\n\r", c, c);
    }
  } while (c != 'q');

  quit_program();
  return 0;
}
