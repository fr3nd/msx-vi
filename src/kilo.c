#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "asm.h"

Z80_registers regs;

#define KILO_VERSION "0.0.1"

#define ARROW_RIGHT 28
#define ARROW_LEFT  29
#define ARROW_UP    30
#define ARROW_DOWN  31
#define PAGE_UP     -1 // MSX doesn't have this key
#define PAGE_DOWN   -2 // MSX doesn't have this key
#define HOME_KEY    11
#define END_KEY     -3 // MSX doesn't have this key
#define DEL_KEY     127

#define _TERM0  0x00
#define _TERM   0x62
#define _DOSVER 0x6F
#define _INITXT 0x006C
#define _POSIT  0xC6

#define perror(x) {printf("*** ");printf(x);printf("\r\n");}
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

typedef struct erow {
  int size;
  char *chars;
} erow;

struct editorConfig {
  int cx, cy;
  int screenrows;
  int screencols;
  int numrows;
  erow row;
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

void editorOpen() {
  char *line = "Hello, world!";
  size_t linelen = 13;
  E.row.size = linelen;
  E.row.chars = malloc(linelen + 1);
  memcpy(E.row.chars, line, linelen);
  E.row.chars[linelen] = '\0';
  E.numrows = 1;
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

  E.cx = 0;
  E.cy = 0;
  E.numrows = 0;
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
    if (y >= E.numrows) {
      if (y == E.screenrows / 3) {
        char welcome[80];
        int welcomelen;
        int padding;

        sprintf(welcome, "Kilo editor -- version %s", KILO_VERSION);
        welcomelen = strlen(welcome);

        padding = (E.screencols - welcomelen) / 2;

        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--) abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
      } else {
        abAppend(ab, "~", 1);
      }
      abAppend(ab, "\33K", 2);

      if (y < E.screenrows - 1) {
        abAppend(ab, "\n\r", 2);
      }
    } else {
      int len = E.row.size;
      if (len > E.screencols) len = E.screencols;
      abAppend(ab, E.row.chars, len);
    }
  }
}

void printBuff(struct abuf *ab) {
  int x;
  for (x = 0; x < ab->len; x++) {
    putchar(ab->b[x]);
  }
}

void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;
  char buf[3];

  abAppend(&ab, "\33x5", 3); // Hide cursor
  abAppend(&ab, "\33H", 2);  // Move cursor to 0,0
  editorDrawRows(&ab);

  abAppend(&ab,"\33Y", 2);
  sprintf(buf, "%c", E.cy + 32);
  abAppend(&ab, buf, 1);
  sprintf(buf, "%c", E.cx + 32);
  abAppend(&ab, buf, 1);

  abAppend(&ab, "\33y5", 3); // Enable cursor

  printBuff(&ab);
  abFree(&ab);
}

/*** input ***/

char editorReadKey() {
  return getchar();
}

void editorMoveCursor(char key) {
  int times;

  switch (key) {
    case PAGE_UP:
    case PAGE_DOWN:
      {
        times = E.screenrows;
        while (times--)
          editorMoveCursor(key == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      }
      break;
    case ARROW_RIGHT:
      if (E.cx != E.screencols - 1) {
        E.cx++;
      }
      break;
    case ARROW_UP:
      if (E.cy != 0) {
        E.cy--;
      }
      break;
    case ARROW_DOWN:
      if (E.cy != E.screenrows - 1) {
        E.cy++;
      }
      break;
  }
}

void editorProcessKeypress() {
  char c = editorReadKey();
  //printf("%d", c); // for getting key code
  switch (c) {
    case CTRL_KEY('q'):
      cls();
      gotoxy(0, 0);
      exit(0);
      break;
    case HOME_KEY:
      E.cx = 0;
      break;
    case END_KEY:
      E.cx = E.screencols - 1;
      break;
    case CTRL_KEY('d'):
      editorMoveCursor(PAGE_DOWN);
      break;
    case CTRL_KEY('u'):
      editorMoveCursor(PAGE_UP);
      break;
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;
  }
}

/*** main ***/

int main() {
  char c;

  init();

  while (1) {
    editorOpen();
    editorRefreshScreen();
    editorProcessKeypress();
  }

  quit_program();
  return 0;
}
