// vim:foldmethod=marker:foldlevel=0
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#define MSXVI_VERSION "0.0.3a"
#define MSXVI_TAB_STOP 8
#define FILE_BUFFER_LENGTH 1024
#define LINE_BUFFER_LENGTH 1024

#define TEXT2_COLOR_TABLE 0x00800

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

/* vi modes */
#define M_COMMAND 0
#define M_INSERT  1

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
#define WRTVDP #0x0047

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
  char mode;
  char full_refresh;
  char welcome_msg;
  char msgbar_updated;
  char *search_pattern;
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
  clear_inverted_area();
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

void vdp_w(char data, char reg) __naked {
  reg;
  data;
  __asm
    push ix
    ld ix,#4
    add ix,sp

    ld b,0(ix)
    ld c,1(ix)
    ld ix,WRTVDP
    BIOSCALL

    pop ix
    ret
  __endasm;
}

void vpoke(unsigned int address, unsigned char value) __naked {
  address;
  value;
  __asm
    push ix
    ld ix,#4
    add ix,sp

    ld l,0(ix)
    ld h,1(ix)
    ld a,2(ix)
    ld ix,NWRVRM
    BIOSCALL

    pop ix
    ret
  __endasm;
}

unsigned char vpeek(unsigned int address) __naked {
  address;
  __asm
    push ix
    ld ix,#4
    add ix,sp

    ld l,0(ix)
    ld h,1(ix)
    ld ix,NRDVRM
    BIOSCALL

    ld h, #0x00
    ld l,a

    pop ix
    ret
  __endasm;
}

void set_blink_colors(char fg, char bg) {
  vdp_w(bg*16+fg, 12);
}

void set_inverted_area(void) {
  int i;

  for (i=0; i < (E.screenrows+2) * E.screencols/8; i++) vpoke(TEXT2_COLOR_TABLE+i, 0x00);
  for (i=0; i < E.screencols/8; i++) vpoke(TEXT2_COLOR_TABLE+i + (E.screencols/8*22), 0xff);
  vdp_w(0x4f, 12); // blink colors
  set_blink_colors(15, 4);
  vdp_w(0xf0, 13); // blink speed: stopped
}

void clear_inverted_area(void) {
  int n;

  // Set back blink table to blank
  for (n=0; n < (E.screenrows+2) * E.screencols/8; n++) vpoke(TEXT2_COLOR_TABLE+n, 0x00);
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

int editorRowRxToCx(erow *row, int rx) {
  int cur_rx = 0;
  int cx;
  for (cx = 0; cx < row->size; cx++) {
    if (row->chars[cx] == '\t')
      cur_rx += (MSXVI_TAB_STOP - 1) - (cur_rx % MSXVI_TAB_STOP);
    cur_rx++;
    if (cur_rx > rx) return cx;
  }
  return cx;
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

  // Update screen
  E.full_refresh = 1;

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

  // Update screen
  E.full_refresh = 1;

  if (at < 0 || at >= E.numrows) return;
  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
  E.numrows--;
  E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c) {
  int n;

  // Update screen
  printf("\33x5");
  putchar(c);
  if (at != row->size) {
    for (n=at; n < row->size; n++) {
      putchar(row->chars[n]);
    }
  }

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
  int n;

  // Update screen
  printf("\33x5");
  if (at != row->size) {
    for (n=at+1; n < row->size; n++) {
      putchar(row->chars[n]);
    }
  }
  printf("\33K");

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

void editorDelChar(char do_backspace) {
  erow *row;

  if (do_backspace)
    printf("\b");

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

void editorOpen(char *filename) {
  int fp;
  char buffer[FILE_BUFFER_LENGTH];
  char line_buffer[LINE_BUFFER_LENGTH];
  int bytes_read;
  int total_read;
  int n, m;

  free(E.filename);
  E.filename = strdup(filename);

  fp  = open(filename, O_RDONLY);
  if (fp < 0) die("Error opening file.");

  m = 0; // Counter for line buffer
  total_read = 0;
  printf("\33x5\33Y6 \33KOpening file: %s", filename);
  while(1) {
    bytes_read = read(buffer, sizeof(buffer), fp);
    total_read = total_read + bytes_read;
    printf("\33Y7 \33K%d bytes read.", total_read);

    for (n=0; n<bytes_read ;n++) {
      if (buffer[n] != '\n' && buffer[n] != '\r') {
        line_buffer[m] = buffer[n];
        m++;
      } else if (buffer[n] == '\n') {
        line_buffer[m] = buffer[n];
        line_buffer[m+1] = '\0';
        editorInsertRow(E.numrows, line_buffer, m);
        m = 0;
      }

      if (m >= LINE_BUFFER_LENGTH) {
        die("Error opening file: line too long.");
      }
    }

    if (bytes_read < sizeof(buffer)) {
      // EOF
      break;
    }
  }

  printf("\33Y6 \33KDone!");
  close(fp);
  E.dirty = 0;
}

int editorSave(char *filename) {
  int fp;
  char line_buffer[LINE_BUFFER_LENGTH];
  int n;
  int bytes_written;
  int total_written;

  if (filename == NULL) {
    editorSetStatusMessage("No current filename");
    return 1;
  }

  printf("\33x5\33Y7 \33K");
  total_written = 0;
  fp = open(E.filename, O_RDWR);
  if (fp >= 0) {
    for (n=0; n < E.numrows; n++) {
      strcpy(line_buffer, E.row[n].chars);
      line_buffer[E.row[n].size] = '\r';
      line_buffer[E.row[n].size+1] = '\n';
      bytes_written = write(line_buffer, E.row[n].size+2, fp);
      total_written = total_written + bytes_written;
      printf("\33Y7 %d bytes written to disk: %s", total_written, E.filename);
    }
    editorSetStatusMessage("%d bytes written to disk: %s Done!", total_written, E.filename);
    E.dirty = 0;
    return 0;
  }
  editorSetStatusMessage("Error saving to disk.");
  return 1;
}

/*** file io }}} ***/

/*** find {{{ ***/

void editorFindCallback(char *query, char direction) {
  char *match;
  erow *row;
  int i;
  int current;
  static int last_match = -1;

  if (query == NULL) {
    if (E.search_pattern) {
    } else {
      return;
    }
  } else {
    free(E.search_pattern);
    E.search_pattern = strdup(query);
    free(query);
  }

  current = last_match;
  for (i = 0; i < E.numrows; i++) {
    current += direction;
    if (current == -1) {
      current = E.numrows - 1;
      editorSetStatusMessage("search hit TOP, continuing at BOTTOM");
    } else if (current == E.numrows) {
      current = 0;
      editorSetStatusMessage("search hit BOTTOM, continuing at TOP");
    }

    row = &E.row[current];
    match = strstr(row->render, E.search_pattern);
    if (match) {
      last_match = current;
      E.cy = current;
      E.cx = editorRowRxToCx(row, match - row->render);
      E.rowoff = E.numrows;
      break;
    }
  }
  editorSetStatusMessage("Pattern not found");
}

void editorFind(char c) {
  char *query;

  if (c == '/') {
    query = editorPrompt("/%s");
    editorFindCallback(query, 1);
  } else if (c == '?') {
    query = editorPrompt("?%s");
    editorFindCallback(query, -1);
  }
  if (query) {
    free(query);
  }
}

/*** find }}} ***/

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

void abAppendGotoxy(struct abuf *ab, char x, char y) {
  char buf[3];

  abAppend(ab,"\33Y", 2);
  sprintf(buf, "%c", y + 32);
  abAppend(ab, buf, 1);
  sprintf(buf, "%c", x + 32);
  abAppend(ab, buf, 1);
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
    E.full_refresh = 1;
  }
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
    E.full_refresh = 1;
  }
  if (E.rx < E.coloff) {
    E.coloff = E.rx;
    E.full_refresh = 1;
  }
  if (E.rx >= E.coloff + E.screencols) {
    E.coloff = E.rx - E.screencols + 1;
    E.full_refresh = 1;
  }
}

void editorDrawRows(struct abuf *ab) {
  int y, n;
  int len;
  char welcome[80];
  int welcomelen;
  int padding;
  int filerow;

  n = 0;
  for (y = 0; y < E.screenrows; y++) {
    filerow = y + E.rowoff;
    if (filerow >= E.numrows) {
      if (E.welcome_msg && E.numrows == 0 && (y >= E.screenrows / 3 && y <= E.screenrows / 3 + 1)) {
        switch (n) {
          case 0:
            sprintf(welcome, "MSX vi -- version %s", MSXVI_VERSION);
            welcomelen = strlen(welcome);
            break;
          case 1:
            sprintf(welcome, "by Carles Amigo (fr3nd)");
            welcomelen = strlen(welcome);
            break;
        }

        n++;
        padding = (E.screencols - welcomelen) / 2;

        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--) abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
      } else {
        if (y>0)
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

  if (E.welcome_msg) {
    E.welcome_msg = 0;
  }
}

void editorDrawStatusBar(struct abuf *ab) {
  char status[80];
  int len;
  char mode;

  switch (E.mode) {
    case M_COMMAND:
      mode = '-';
      break;
    case M_INSERT:
      mode = 'I';
      break;
  }

  len = sprintf(status, "%c %.20s %s %d/%d\33K",
    mode,
    E.filename ? E.filename : "No file",
    E.dirty ? "[Modified]" : "",
    E.cy + 1, E.numrows);
  abAppendGotoxy(ab, 0, 22);
  abAppend(ab, status, len);

}

void editorDrawMessageBar(struct abuf *ab) {
  int msglen;

  if (E.msgbar_updated) {
    E.msgbar_updated = 0;
    abAppendGotoxy(ab, 0, 23);
    msglen = strlen(E.statusmsg);
    if (msglen > E.screencols) msglen = E.screencols;
    if (msglen) {
      abAppend(ab, E.statusmsg, msglen);
      E.statusmsg[0] = '\0';
      E.msgbar_updated = 1;
    }
    abAppend(ab, "\33K", 2);
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

  editorScroll();

  abAppend(&ab, "\33x5", 3); // Hide cursor
  abAppend(&ab, "\33H", 2);  // Move cursor to 0,0

  if (E.full_refresh) {
    E.full_refresh = 0;
    editorDrawRows(&ab);
  }
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  abAppendGotoxy(&ab, E.rx - E.coloff, E.cy - E.rowoff);

  abAppend(&ab, "\33y5", 3); // Enable cursor


  printBuff(&ab);
  abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsprintf(E.statusmsg, fmt, ap);
  va_end(ap);
  E.msgbar_updated = 1;
}
/*** output }}} ***/

/*** input {{{ ***/

void runCommand() {
  char *command;
  char *token;
  char f[13];
  char params[40];
  int n;

  command = editorPrompt(":%s");

  if (command != NULL && strlen(command) > 0) {
    if (strcmp(command, "q") == 0) {
      if (E.dirty) {
        editorSetStatusMessage("No write since last change (:q! overrides)");
      } else {
        quit_program(0);
      }
    } else if (strcmp(command, "w") == 0) {
      editorSave(E.filename);
    } else if (strncmp(command, "w ", 2) == 0) {
      strncpy(f, command + 2, 12);
      editorSave(f);
    } else if (strcmp(command, "q!") == 0) {
      quit_program(0);
    } else if (strcmp(command, "wq") == 0) {
      if (editorSave(E.filename) == 0) {
        quit_program(0);
      }
    } else if (strncmp(command, "color", 5) == 0) {
      // Setting default colors
      params[0] = 15;
      params[1] = 4;
      params[2] = 4;

      n = 0;
      token = strtok(command, " ");
      while( token != NULL ) {
        token = strtok(NULL, " ");
        if (atoi(token) > 0 && atoi(token) <= 15) {
          params[n] = atoi(token);
        }
        n++;
      }
      color(params[0], params[1], params[2]);
      set_blink_colors(params[0], params[1]);

    } else {
      editorSetStatusMessage("Not an editor command: %s", command);
    }
  } else {
    editorSetStatusMessage("");
  }
}

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
      free(buf);
      return NULL;
    } else if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");
        return buf;
      } else {
        free(buf);
        return NULL;
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
    case HOME_KEY:
      E.cx = 0;
      break;
    case END_KEY:
      if (E.cy < E.numrows)
        E.cx = E.row[E.cy].size;
      break;
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
      } else if (E.cy > 0 && E.mode != M_COMMAND) {
        E.cy--;
        E.cx = E.row[E.cy].size;
      }
      break;
    case ARROW_RIGHT:
      if (row && E.cx < row->size) {
        E.cx++;
      } else if (E.mode == M_INSERT && E.cx == 0 && row->size == 0) {
        E.cx++;
      } else if (row && E.cx == row->size && E.mode != M_COMMAND) {
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

  if (E.cx == rowlen && E.mode == M_COMMAND) {
    E.cx--;
  }

  if (E.cx < 0)
    E.cx = 0;
  if (E.cy < 0)
    E.cy = 0;

}

void editorProcessKeypress() {
  char c;
  
  c = editorReadKey();

  if (E.mode == M_COMMAND) {
    switch (c) {
      case CTRL_KEY('d'):
        editorMoveCursor(PAGE_DOWN);
        break;
      case CTRL_KEY('u'):
        editorMoveCursor(PAGE_UP);
        break;
      case 'k':
        editorMoveCursor(ARROW_UP);
        break;
      case 'j':
        editorMoveCursor(ARROW_DOWN);
        break;
      case 'l':
        editorMoveCursor(ARROW_RIGHT);
        break;
      case 'h':
        editorMoveCursor(ARROW_LEFT);
        break;
      case ARROW_UP:
      case ARROW_DOWN:
      case ARROW_LEFT:
      case ARROW_RIGHT:
        editorMoveCursor(c);
        break;
      case CTRL_KEY('l'):
        E.full_refresh = 1;
        editorRefreshScreen();
        break;
      case ESC:
        printf("\a"); // BEEP
        break;
      case '0':
        editorMoveCursor(HOME_KEY);
        break;
      case '$':
        editorMoveCursor(END_KEY);
        break;
      case ':':
        runCommand();
        break;
      case '/': // Search word
        editorFind('/');
        break;
      case '?': // Search word backwards
        editorFind('?');
        break;
      case 'n': // Next search (forward)
        // TODO when searching with ? forward should go backwards
        editorFindCallback(NULL, 1);
        break;
      case 'N': // Previous search (backwards)
        // TODO when searching with ? backward should go forward
        editorFindCallback(NULL, -1);
        break;
      case 'a':
        E.mode = M_INSERT;
        editorMoveCursor(ARROW_RIGHT);
        break;
      case 'A':
        E.mode = M_INSERT;
        editorMoveCursor(END_KEY);
        break;
      case 'i':
        E.mode = M_INSERT;
        break;
      case 'I':
        editorMoveCursor(HOME_KEY);
        E.mode = M_INSERT;
        break;
      case 'o':
        E.mode = M_INSERT;
        editorMoveCursor(HOME_KEY);
        editorMoveCursor(ARROW_DOWN);
        editorInsertNewline();
        editorMoveCursor(ARROW_UP);
        break;
      case 'O':
        E.mode = M_INSERT;
        editorMoveCursor(HOME_KEY);
        editorInsertNewline();
        editorMoveCursor(ARROW_UP);
        break;
      case 'x':
        editorMoveCursor(ARROW_RIGHT);
        editorDelChar(0);
        break;
      case 'X':
        editorDelChar(1);
        break;
    }
  } else if (E.mode == M_INSERT) {
    switch (c) {
      case '\r':
        editorInsertNewline();
        break;
      case BACKSPACE:
      case DEL_KEY:
        if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
        editorDelChar(1);
        break;
      case ESC:
        E.mode = M_COMMAND;
        editorMoveCursor(ARROW_LEFT);
        break;
      case HOME_KEY:
      case END_KEY:
      case ARROW_UP:
      case ARROW_DOWN:
      case ARROW_LEFT:
      case ARROW_RIGHT:
        editorMoveCursor(c);
        break;
      default:
        editorInsertChar(c);
        break;
    }
  }
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
  E.mode = M_COMMAND;
  E.full_refresh = 1;
  E.welcome_msg = 1;
  E.msgbar_updated = 1;
  E.search_pattern = NULL;
}


void quit_program(int exit_code) {
  int n;

  cls();
  clear_inverted_area();
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

  // Set inverted text area
  set_inverted_area();

  if (filename[0] != '\0') {
    editorOpen(filename);
  }

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  quit_program(0);
  return 0;
}

/*** main }}} ***/
