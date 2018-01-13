#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "asm.h"

Z80_registers regs;

#define _TERM0  0x00
#define _TERM   0x62
#define _DOSVER 0x6F
#define _INITXT 0x006C
#define _POSIT  0xC6

#define perror(x) {printf("*** ");printf(x);printf("\r\n");}
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct editorConfig {
  int screenrows;
  int screencols;
};

struct editorConfig E;

/*** functions ***/

void gotoxy(int x, int y) {
  regs.Bytes.H = x+1;
  regs.Bytes.L = y+1;
  BiosCall(_POSIT, &regs, REGS_AF);
}

void cls(void) {
  putchar(0x1b);
  putchar(0x45);
}

void exit(int code) {
  regs.Bytes.B = code;
  DosCall(_TERM, &regs, REGS_MAIN, REGS_NONE);
}

void die(const char *s) {
  cls();
  gotoxy(0, 0);
  perror(s);
  exit(1);
}

int getCursorPosition(int *rows, int *cols) {
  static char __at(0xF3DC) _CSRY;
  static char __at(0xF3DD) _CSRX;

  *rows = _CSRY;
  *cols = _CSRX;

  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  gotoxy(999, 999);
  return getCursorPosition(rows, cols);
}

void init() {
  static char __at(0xF3AE) _LINL40;

  // Check MSX-DOS version >= 2
  DosCall(_DOSVER, &regs, REGS_NONE, REGS_MAIN);
  if(regs.Bytes.B < 2) {
    die("This program requires MSX-DOS 2 to run.");
  }

  // Set 80 column mode
  _LINL40 = 80;
  BiosCall(_INITXT, &regs, REGS_NONE);

  // Get window size
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");

}

void quit_program() {
}

/*** append buffer ***/

struct abuf {
  char *b;
  int len;
};
#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}
void abFree(struct abuf *ab) {
  free(ab->b);
}

/*** output ***/

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    abAppend(ab, "~", 1);

    if (y < E.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }

  }
  abAppend(ab, "\0", 1);
}

void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;

  cls();
  gotoxy(0, 0);

  editorDrawRows(&ab);
  printf(ab.b);

  gotoxy(0, 0);
  abFree(&ab);
}

/*** input ***/

char editorReadKey() {
  return getchar();
}

void editorProcessKeypress() {
  char c = editorReadKey();
  switch (c) {
    case CTRL_KEY('q'):
      cls();
      gotoxy(0, 0);
      exit(0);
      break;
  }
}

/*** main ***/

int main() {
  char c;

  init();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  quit_program();
  return 0;
}
