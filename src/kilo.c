#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include "asm.h"
#include "heap.h"

Z80_registers regs;

#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8
#define FILE_BUFFER_LENGTH 512

#define BACKSPACE   8
#define ESC         27
#define ARROW_RIGHT 28
#define ARROW_LEFT  29
#define ARROW_UP    30
#define ARROW_DOWN  31
#define PAGE_UP     -1 // MSX doesn't have this key
#define PAGE_DOWN   -2 // MSX doesn't have this key
#define HOME_KEY    11
#define END_KEY     -3 // MSX doesn't have this key
#define DEL_KEY     127

/* open/creat flags */
#define  O_RDONLY   0x01
#define  O_WRONLY   0x02
#define  O_RDWR     0x00
#define  O_INHERIT  0x04

/* DOS calls */
#define _TERM0  0x00
#define _TERM   0x62
#define _DOSVER 0x6F
#define _INITXT 0x006C
#define _POSIT  0xC6
#define _OPEN   0x43
#define _CLOSE  0x45
#define _READ   0x48
#define _WRITE  0x49
#define _EOF    0xC7
#define _GTIME  0x2C
#define _GDATE  0x2A

/* BIOS calls */
#define _CHGET  0x9F

#define perror(x) {printf("*** ");printf(x);printf("\r\n");}
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

typedef struct erow {
  int size;
  int rsize;
  char *chars;
  char *render;
} erow;

struct editorConfig {
  int cx, cy;
  int rx;
  int rowoff;
  int coloff;
  int screenrows;
  int screencols;
  int numrows;
  erow *row;
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
};

struct editorConfig E;

/*** prototypes ***/

void editorSetStatusMessage(const char *fmt, ...);

/*** functions ***/

// https://stackoverflow.com/questions/252782/strdup-what-does-it-do-in-c

char _getchar(void) {
  BiosCall(_CHGET, &regs, REGS_AF);
  return regs.Bytes.A;
}

char *strdup (const char *s) {
  char *d = malloc (strlen (s) + 1);   // Space for length plus nul
  if (d == NULL) return NULL;          // No memory
  strcpy (d,s);                        // Copy the characters
  return d;                            // Return the new string
}

void gotoxy(int x, int y) {
  regs.Bytes.H = x+1;
  regs.Bytes.L = y+1;
  BiosCall(_POSIT, &regs, REGS_AF);
}

void cls(void) {
  putchar(0x1b);
  putchar(0x45);
}

int open(char *fn, int mode) {
  regs.Words.DE = (int)fn;
  regs.Bytes.A = mode;
  regs.Bytes.B = 0;
  DosCall(_OPEN, &regs, REGS_MAIN, REGS_MAIN);
  if (regs.Bytes.A == 0) {
    return regs.Bytes.B;
  } else {
    return -1;
  }
}

int close(int fp) {
  regs.UWords.DE = fp;
  DosCall(_CLOSE, &regs, REGS_MAIN, REGS_NONE);
  return regs.Bytes.B;
}

void exit(int code) {
  regs.Bytes.B = code;
  DosCall(_TERM, &regs, REGS_MAIN, REGS_NONE);
}

time_t _time() {
  char h, m, s, mo, d;
  int y;

  DosCall(_GTIME, &regs, REGS_MAIN, REGS_MAIN);
  h = regs.Bytes.H;
  m = regs.Bytes.L;
  s = regs.Bytes.D;

  DosCall(_GDATE, &regs, REGS_MAIN, REGS_MAIN);
  y = regs.Words.HL;
  mo = regs.Bytes.D;
  d = regs.Bytes.E;

  /* XXX Calculating basic not acurate timestamp. */
  return (((( ((y-1970)*365) + (mo * 30) + d ) * 24 + h) * 60 + m) * 60 + s);
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
  gotoxy(999, 999);
  return getCursorPosition(rows, cols);
}

/*** row operations ***/

int editorRowCxToRx(erow *row, int cx) {
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    if (row->chars[j] == '\t')
      rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
    rx++;
  }
  return rx;
}

void editorUpdateRow(erow *row) {
  int tabs = 0;
  int idx = 0;
  int j;

  for (j = 0; j < row->size; j++)
    if (row->chars[j] == '\t') tabs++;

  free(row->render);
  row->render = malloc(row->size + tabs*(KILO_TAB_STOP - 1) + 1);

  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % KILO_TAB_STOP != 0) row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;
}

void editorAppendRow(char *s, size_t len) {
  int at;

  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  editorUpdateRow(&E.row[at]);
  E.numrows++;
}

void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size) at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
}

/*** editor operations ***/

void editorInsertChar(int c) {
  if (E.cy == E.numrows) {
    editorAppendRow("", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}

/*** file io ***/

int read(char* buf, uint size, byte fp) {
  regs.Bytes.B = fp;
  regs.UWords.DE = (uint)buf;
  regs.UWords.HL = size;
  DosCall(_READ, &regs, REGS_MAIN, REGS_MAIN);

  if (regs.Bytes.A == 0) {
    return regs.UWords.HL;
  } else {
    return -1;
  }
}

int write(char* buf, uint size, byte fp) {
  regs.Bytes.B = fp;
  regs.UWords.DE = (int)buf;
  regs.UWords.HL = size;
  DosCall(_WRITE, &regs, REGS_MAIN, REGS_MAIN);

  if (regs.Bytes.A == 0) {
    return regs.UWords.HL;
  } else {
    return -1;
  }
}

//* Read one char from file
char fgetc(int fp) {
  char c[1];

  printf(".");
  if ( read(c, sizeof(char), fp) != sizeof(char) ) {
    return EOF;
  } else {
    return *c;
  }
}

//* Read line from file
char *fgets(char *s, int n, int fp) {
  int c = 0;
  char *cptr;

  cptr = s;

  while (--n > 0 && (c = fgetc(fp)) != EOF) {
    if (c != '\n') {
      if (c != '\r') {
        *cptr++ = c;
      }
    } else {
      break;
    }
  }
  *cptr = '\0';
  return (c == EOF && cptr == s)? NULL: s;
}

char *editorRowsToString(int *buflen) {
  int totlen = 0;
  int j;
  char *buf;
  char *p;

  for (j = 0; j < E.numrows; j++)
    totlen += E.row[j].size + 1;
  *buflen = totlen;
  buf = malloc(totlen);
  p = buf;
  for (j = 0; j < E.numrows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    //strcpy(p, E.row[j].chars);
    p += E.row[j].size;
    *p = '\n';
    p++;
    *p = '\r';
    p++;
  }
  return buf;
}

void editorOpen(char *filename) {
  int fp;
  char buffer[FILE_BUFFER_LENGTH+1];
  //int size_read;
  
  free(E.filename);
  E.filename = strdup(filename);

  fp  = open(filename, O_RDONLY);
  if (fp < 0) die("Error opening file");
  while (fgets(buffer, FILE_BUFFER_LENGTH, fp) != NULL) {
    editorAppendRow(buffer, strlen(buffer));
  }

  close(fp);
}

void editorSave() {
  int len;
  char *buf;
  int fd;
  int ret;

  if (E.filename == NULL) return;
  buf = editorRowsToString(&len);
  fd = open(E.filename, O_RDWR);
  if (fd != -1) {
    if (write(buf, len, fd) == len) {
      close(fd);
      free(buf);
      editorSetStatusMessage("%d bytes written to disk", len);
      return;
    }
    close(fd);
  }
  free(buf);
  // TODO: Implement error message
  //editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
  editorSetStatusMessage("Can't save! I/O error");
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
  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    die("ERROR: Could not get screen size");
  E.screenrows -= 2;
  gotoxy(0, 0);

  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;
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

void editorScroll() {
  E.rx = 0;
  if (E.cy < E.numrows) {
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  }

  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.rx < E.coloff) {
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + E.screencols) {
    E.coloff = E.rx - E.screencols + 1;
  }
}

void editorDrawRows(struct abuf *ab) {
  int y;
  int len;
  char welcome[80];
  int welcomelen;
  int padding;
  int filerow;

  for (y = 0; y < E.screenrows; y++) {
    filerow = y + E.rowoff;
    if (filerow >= E.numrows) {
      if (E.numrows == 0 && y == E.screenrows / 3) {

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
    } else {
      len = E.row[filerow].rsize - E.coloff;
      if (len < 0) len = 0;
      if (len > E.screencols) len = E.screencols;
      abAppend(ab, &E.row[filerow].render[E.coloff], len);
    }

    abAppend(ab, "\33K", 2);
    abAppend(ab, "\n\r", 2);
  }
}

void editorDrawStatusBar(struct abuf *ab) {
  char status[80], rstatus[80];
  int len, rlen;

  //abAppend(ab, "\x1b[7m", 4); // MSX doesn't support inverted text
  
  len = sprintf(status, "%.20s - %d lines", E.filename ? E.filename : "[No Name]", E.numrows);
  rlen = sprintf(rstatus, "%d/%d", E.cy + 1, E.numrows);
  if (len > E.screencols) len = E.screencols;
  abAppend(ab, status, len);
 
  while (len < E.screencols) {
    if (E.screencols - len - 1 == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      len++;
    }
  }
  //abAppend(ab, "\x1b[m", 3); // MSX doesn't support inverted text
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
  int msglen;

  abAppend(ab, "\33K", 2);
  msglen = strlen(E.statusmsg);
  if (msglen > E.screencols) msglen = E.screencols;
  if (msglen && _time() - E.statusmsg_time < 5)
    abAppend(ab, E.statusmsg, msglen);
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

  editorScroll();

  abAppend(&ab, "\33x5", 3); // Hide cursor
  abAppend(&ab, "\33H", 2);  // Move cursor to 0,0

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  abAppend(&ab,"\33Y", 2);
  sprintf(buf, "%c", (E.cy - E.rowoff) + 32);
  abAppend(&ab, buf, 1);
  sprintf(buf, "%c", (E.rx - E.coloff) + 32);
  abAppend(&ab, buf, 1);

  abAppend(&ab, "\33y5", 3); // Enable cursor


  printBuff(&ab);
  abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsprintf(E.statusmsg, fmt, ap);
  va_end(ap);
  E.statusmsg_time = _time();
}

/*** input ***/

char editorReadKey() {
  return _getchar();
}

void editorMoveCursor(char key) {
  int times;
  int rowlen;
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

  switch (key) {
    case PAGE_UP:
    case PAGE_DOWN:
      {
        if (key == PAGE_UP) {
          E.cy = E.rowoff;
        } else if (key == PAGE_DOWN) {
          E.cy = E.rowoff + E.screenrows - 1;
          if (E.cy > E.numrows) E.cy = E.numrows;
        }

        times = E.screenrows;
        while (times--)
          editorMoveCursor(key == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      } else if (E.cy > 0) {
        E.cy--;
        E.cx = E.row[E.cy].size;
      }
      break;
    case ARROW_RIGHT:
      if (row && E.cx < row->size) {
        E.cx++;
      } else if (row && E.cx == row->size) {
        E.cy++;
        E.cx = 0;
      }
      break;
    case ARROW_UP:
      if (E.cy != 0) {
        E.cy--;
      }
      break;
    case ARROW_DOWN:
      if (E.cy < E.numrows) {
        E.cy++;
      }
      break;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  rowlen = row ? row->size : 0;
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}

void editorProcessKeypress() {
  char c = editorReadKey();
  //printf("%d", c); // for getting key code
  switch (c) {
    case '\r':
      // TODO
      break;
    case CTRL_KEY('q'):
      cls();
      gotoxy(0, 0);
      exit(0);
      break;
    case CTRL_KEY('s'):
      editorSave();
      break;
    case HOME_KEY:
      E.cx = 0;
      break;
    case END_KEY:
      if (E.cy < E.numrows)
        E.cx = E.row[E.cy].size;
      break;
    case BACKSPACE:
    case DEL_KEY:
      // TODO
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

    case CTRL_KEY('l'):
      break;
    case ESC:
      printf("\a"); // BEEP
      break;
    default:
      editorInsertChar(c);
      break;
  }
}

/*** main ***/

void debug_keys(void) {
  char c;
  c = _getchar();
  printf("%d\n\r", c);
}

int main(char **argv, int argc) {
  int i;
  char filename[13] ;
  *filename = '\0';

  /*while (1) debug_keys();*/

  init();

  for (i = 0; i < argc; i++) {
    if (argv[i][0] == '-') {
      switch (argv[i][1]) {
        /*case 'h':*/
          /*usage();*/
          /*break;*/
        default:
          die("ERROR: Parameter not recognised");
          break;
      }
    } else {
      strcpy(filename, argv[i]);
    }
  }

  if (filename[0] != '\0') {
    editorOpen(filename);
  }

  editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit");

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  quit_program();
  return 0;
}
