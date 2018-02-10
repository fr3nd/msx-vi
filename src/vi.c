// vim:foldmethod=marker:foldlevel=0
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#define MSXVI_VERSION "0.0.1a"
#define MSXVI_TAB_STOP 8
#define MSXVI_QUIT_TIMES 3
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
#define TERM    #0x62
#define DOSVER  #0x6F
#define OPEN    #0x43
#define CLOSE   #0x45
#define READ    #0x48
#define WRITE   #0x49
#define _EOF    0xC7
#define GTIME   #0x2C
#define GDATE   #0x2A

/* BIOS calls */
#define CHGET  #0x009F
#define CHPUT  #0x00A2
#define CALSLT #0x001C
#define EXPTBL #0xFCC1
#define POSIT  #0x00C6
#define INITXT #0x006C
#define CLS    #0x00C3
#define CHGCLR #0x0062
#define CHGMOD #0x005F
#define WRTVRM #0x004D
#define NRDVRM #0x0174
#define NWRVRM #0x0177
#define FILVRM #0x0056
#define DISSCR #0x0041
#define ENASCR #0x0044

/* Memory variables */
#define LINL40 0xF3AE
#define FORCLR 0xF3E9
#define BAKCLR 0xF3EA
#define BDRCLR 0xF3EB

#define perror(x) {printf("*** ");printf(x);printf("\r\n");}
#define CTRL_KEY(k) ((k) & 0x1f)
#define DOSCALL  call 5
#define BIOSCALL ld iy,(EXPTBL-1)\
call CALSLT

/*** data {{{ ***/

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
  int dirty;
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
};

typedef struct {
  char hour;  /* Hours 0..23 */
  char min; /* Minutes 0..59 */
  char sec; /* Seconds 0..59 */
} TIME;

typedef struct {
  int year; /* Year 1980...2079 */
  char month; /* Month 1=Jan..12=Dec */
  char day; /* Day of the month 1...31 */
  char dow; /* On getdate() gets Day of week 0=Sun...6=Sat */
} DATE;

/*** end data }}} ***/

/*** global variables {{{ ***/

struct editorConfig E;


/*** end global variables }}} ***/

/*** prototypes {{{ ***/

void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt);
void quit_program(int exit_code);

/*** end prototypes }}} ***/

/*** functions {{{ ***/

char dosver(void) __naked {
  __asm
    push ix

    ld c, DOSVER
    DOSCALL

    ld h, #0x00
    ld l, b

    pop ix
    ret
  __endasm;
}

char getchar(void) __naked {
  __asm
    push ix

    ld ix,CHGET
    BIOSCALL
    ld h, #0x00
    ld l,a ;reg to put a read character

    pop ix
    ret
  __endasm;
}

void putchar(char c) __naked {
  c;
  __asm
    push ix
    ld ix,#4
    add ix,sp

    ld a,(ix)
    ld ix,CHPUT
    BIOSCALL

    pop ix
    ret
  __endasm;
}

void initxt(char columns) __naked {
  columns;
  __asm
    push ix
    ld ix,#4
    add ix,sp

    ld a,(ix)
    ld (LINL40),a

    ld ix,INITXT
    BIOSCALL

    pop ix
    ret
  __endasm;
}

// https://stackoverflow.com/questions/252782/strdup-what-does-it-do-in-c
char *strdup (const char *s) {
  char *d = malloc (strlen (s) + 1);   // Space for length plus nul
  if (d == NULL) return NULL;          // No memory
  strcpy (d,s);                        // Copy the characters
  return d;                            // Return the new string
}

void gotoxy(char x, char y) __naked {
  x;
  y;
  __asm
    push ix
    ld ix,#4
    add ix,sp

    ld l,(ix)
    inc l
    ld h,1(ix)
    inc h
    ld ix,POSIT
    BIOSCALL

    pop ix
    ret
  __endasm;
}

void cls(void) __naked {
  __asm
    push ix
    cp a
    ld ix,CLS
    BIOSCALL
    pop ix
    ret
  __endasm;
}

int open(char *fn, int mode) __naked {
  fn;
  mode;
  __asm
    push ix
    ld ix,#4
    add ix,sp

    ld e,0(ix)
    ld d,1(ix)
    ld a,2(ix)
    ld c, OPEN
    DOSCALL

    cp #0
    jr z, open_noerror$
    ld h, #0xFF
    ld l, #0xFF
    jp open_error$
  open_noerror$:
    ld h, #0x00
    ld l, b
  open_error$:
    pop ix
    ret
  __endasm;
}

int close(int fp) __naked {
  fp;
  __asm
    push ix
    ld ix,#4
    add ix,sp

    ld b,(ix)
    ld c, CLOSE
    DOSCALL

    ld h, #0x00
    ld l,a

    pop ix
    ret
  __endasm;
}

void exit(int code) __naked {
  code;
  __asm
    push ix
    ld ix,#4
    add ix,sp

    ld b,(ix)
    ld c, TERM
    DOSCALL

    pop ix
    ret
  __endasm;
}

void gtime(TIME* t) __naked {
  t;
  __asm
    push ix
    ld ix,#4
    add ix,sp

    ld l,0(ix)
    ld h,1(ix)
    ld d,2(ix)
    push hl

    ld c, GTIME
    DOSCALL

    pop ix

    ld 0(ix),h
    ld 1(ix),l
    ld 2(ix),d

    pop ix
    ret
  __endasm;
}

void gdate(DATE* d) __naked {
  d;
  __asm
    push ix
    ld ix,#4
    add ix,sp

    ld l,0(ix)
    ld h,1(ix)
    push hl

    ld c, GDATE
    DOSCALL

    pop ix

    ld 0(ix),l
    ld 1(ix),h
    ld 2(ix),d
    ld 3(ix),e
    ld 4(ix),a

    pop ix
    ret
  __endasm;
}

time_t _time() {
  TIME time;
  DATE date;

  gtime(&time);
  gdate(&date);
  /* XXX Calculating basic not acurate timestamp. */
  return (((( ((date.year-1970)*365) + (date.month * 30) + date.day ) * 24 + time.hour) * 60 + time.min) * 60 + time.sec);
}

void die(const char *s) {
  cls();
  gotoxy(0, 0);
  perror(s);
  quit_program(1);
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

/*** end functions }}} ***/

/*** graphic functions {{{ ***/

void color(char fg, char bg, char bd) __naked {
  fg;
  bg;
  bd;
  __asm
    push ix
    ld ix,#4
    add ix,sp

    ld a,0(ix)
    ld(#FORCLR),a
    ld a,1(ix)
    ld(#BAKCLR),a
    ld a,2(ix)
    ld(#BDRCLR),a

    ld ix,CHGCLR
    BIOSCALL

    pop ix
    ret
  __endasm;
}

/*** end graphic functions }}} ***/

/*** row operations {{{ ***/

int editorRowCxToRx(erow *row, int cx) {
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    if (row->chars[j] == '\t')
      rx += (MSXVI_TAB_STOP - 1) - (rx % MSXVI_TAB_STOP);
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
  row->render = malloc(row->size + tabs*(MSXVI_TAB_STOP - 1) + 1);

  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % MSXVI_TAB_STOP != 0) row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;
}

void editorInsertRow(int at, char *s, size_t len) {
  if (at < 0 || at > E.numrows) return;
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));

  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  editorUpdateRow(&E.row[at]);
  E.numrows++;
  E.dirty++;
}

void editorFreeRow(erow *row) {
  free(row->render);
  free(row->chars);
}
void editorDelRow(int at) {
  if (at < 0 || at >= E.numrows) return;
  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
  E.numrows--;
  E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size) at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row->size) return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  editorUpdateRow(row);
  E.dirty++;
}

/*** end row operations }}} ***/

/*** editor operations {{{ ***/

void editorInsertChar(int c) {
  if (E.cy == E.numrows) {
    editorInsertRow(E.numrows, "", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}

void editorInsertNewline() {
  if (E.cx == 0) {
    editorInsertRow(E.cy, "", 0);
  } else {
    erow *row = &E.row[E.cy];
    editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
    row = &E.row[E.cy];
    row->size = E.cx;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
  }
  E.cy++;
  E.cx = 0;
}

void editorDelChar() {
  erow *row;

  if (E.cy == E.numrows) return;
  if (E.cx == 0 && E.cy == 0) return;
  row = &E.row[E.cy];
  if (E.cx > 0) {
    editorRowDelChar(row, E.cx - 1);
    E.cx--;
  } else {
    E.cx = E.row[E.cy - 1].size;
    editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
    editorDelRow(E.cy);
    E.cy--;
  }
}

/*** editor operations }}} ***/

/*** file io {{{ ***/

int read(char* buf, unsigned int size, char fp) __naked {
  buf;
  size;
  fp;
  __asm
    push ix
    ld ix,#4
    add ix,sp

    ld e,0(ix)
    ld d,1(ix)
    ld l,2(ix)
    ld h,3(ix)
    ld b,4(ix)
    ld c, READ
    DOSCALL

    cp #0
    jr z, read_noerror$
    ld h, #0xFF
    ld l, #0xFF
  read_noerror$:
    pop ix
    ret
  __endasm;
}

int write(char* buf, unsigned int size, char fp) __naked {
  buf;
  size;
  fp;
  __asm
    push ix
    ld ix,#4
    add ix,sp

    ld e,0(ix)
    ld d,1(ix)
    ld l,2(ix)
    ld h,3(ix)
    ld b,4(ix)
    ld c, WRITE
    DOSCALL

    cp #0
    jr z, write_noerror$
    ld h, #0xFF
    ld l, #0xFF
  write_noerror$:
    pop ix
    ret
  __endasm;
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

  free(E.filename);
  E.filename = strdup(filename);

  fp  = open(filename, O_RDONLY);
  if (fp < 0) die("Error opening file");
  while (fgets(buffer, FILE_BUFFER_LENGTH, fp) != NULL) {
    editorInsertRow(E.numrows, buffer, strlen(buffer));
  }

  close(fp);
  E.dirty = 0;
}

void editorSave() {
  int len;
  char *buf;
  int fd;
  int ret;

  if (E.filename == NULL) {
    E.filename = editorPrompt("Save as: %s (ESC to cancel)");
    if (E.filename == NULL) {
      editorSetStatusMessage("Save aborted");
      return;
    }
  }

  if (E.filename == NULL) return;
  buf = editorRowsToString(&len);
  fd = open(E.filename, O_RDWR);
  if (fd != -1) {
    if (write(buf, len, fd) == len) {
      close(fd);
      free(buf);
      E.dirty = 0;
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

/*** file io }}} ***/

/*** append buffer {{{ ***/

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


/*** append buffer }}} ***/

/*** output {{{ ***/

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

        sprintf(welcome, "MSX vi -- version %s", MSXVI_VERSION);
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
  
  len = sprintf(status, "%.20s - %d lines %s",
    E.filename ? E.filename : "[No Name]", E.numrows,
    E.dirty ? "(modified)" : "");
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
/*** output }}} ***/

/*** input {{{ ***/

char *editorPrompt(char *prompt) {
  size_t bufsize = 128;
  char *buf = malloc(bufsize);
  size_t buflen = 0;
  int c;

  buf[0] = '\0';
  while (1) {
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();
    c = editorReadKey();
    if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
      if (buflen != 0) buf[--buflen] = '\0';
    } else if (c == ESC) {
      editorSetStatusMessage("");
      free(buf);
      return NULL;
    } else if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }
  }
}

char editorReadKey() {
  return getchar();
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
  static int quit_times = MSXVI_QUIT_TIMES;
  char c = editorReadKey();
  //printf("%d", c); // for getting key code
  switch (c) {
    case '\r':
      editorInsertNewline();
      break;
    case CTRL_KEY('q'):
      if (E.dirty && quit_times > 0) {
        editorSetStatusMessage("WARNING!!! File has unsaved changes. "
          "Press Ctrl-Q %d more times to quit.", quit_times);
        quit_times--;
        return;
      }
      cls();
      gotoxy(0, 0);
      quit_program(0);
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
      if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
      editorDelChar();
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
  quit_times = MSXVI_QUIT_TIMES;
}

/*** input }}} ***/

/*** main {{{ ***/

void init() {
  // Check MSX-DOS version >= 2
  if(dosver() < 2) {
    die("This program requires MSX-DOS 2 to run.");
  }

  // Set 80 column mode
  initxt(80);

  // Get window size
  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    die("ERROR: Could not get screen size");
  E.screenrows -= 2;

  // Set coords to 0,0
  gotoxy(0, 0);

  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;
  E.dirty = 0;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;
}


void quit_program(int exit_code) {
  initxt(80);
  exit(exit_code);
}

void debug_keys(void) {
  char c;
  c = getchar();
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

  quit_program(0);
  return 0;
}

/*** main }}} ***/
