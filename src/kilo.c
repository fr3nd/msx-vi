// vim:foldmethod=marker:foldlevel=0
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include "heap.h"
#include "VDPgraph2.h"

#define MSXVI_VERSION "0.0.1"
#define MSXVI_TAB_STOP 8
#define MSXVI_QUIT_TIMES 3
#define FILE_BUFFER_LENGTH 512

#define SCREEN7_PATTERN 0x00000
#define SCREEN7_SPRITES 0x0F000
#define SCREEN7_SPRCOLR 0x0F800
#define SCREEN7_SPRATTR 0x0FA00
#define SCREEN7_BACKBUF 0x0D400
#define SCREEN7_OFFSETX 0
#define SCREEN7_OFFSETY 0
#define SCREEN7_SIZEX 512
#define SCREEN7_SIZEY 212
#define CHAR_SIZEX 3
#define CHAR_SIZEY 8
#define BACKBUFFER_LENGTH 85

#define TRANSPARENT    0x00
#define BLACK          0x01
#define MEDIUM_GREEN   0x02
#define LIGHT_GREEN    0x03
#define DARK_BLUE      0x04
#define LIGHT_BLUE     0x05
#define DARK_RED       0x06
#define CYAN           0x07
#define MEDIUM_RED     0x08
#define LIGHT_RED      0x09
#define DARK_YELLOW    0x0A
#define LIGHT_YELLOW   0x0B
#define DARK_GREEN     0x0C
#define MAGENTA        0x0D
#define GRAY           0x0E
#define WHITE          0x0F

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

/* Vram positions */
#define CHARTABLE 0x01000 // 80 cols

#define perror(x) {printf("*** ");printf(x);printf("\r\n");}
#define CTRL_KEY(k) ((k) & 0x1f)
#define DOSCALL  call 5
#define BIOSCALL ld iy,(EXPTBL-1)\
call CALSLT

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? 'X' : ' '), \
  (byte & 0x40 ? 'X' : ' '), \
  (byte & 0x20 ? 'X' : ' '), \
  (byte & 0x10 ? 'X' : ' '), \
  (byte & 0x08 ? 'X' : ' '), \
  (byte & 0x04 ? 'X' : ' '), \
  (byte & 0x02 ? 'X' : ' '), \
  (byte & 0x01 ? 'X' : ' ')

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

struct coord {
  int x, y;
};

char fgcolor;
char bgcolor;
char inverted;

struct coord cursor_pos;
struct editorConfig E;
unsigned char char_table[8*255];
char escape_sequence = 0;
char escape_sequence_2 = 0;
char escape_sequence_y = 0;


/*** end global variables }}} ***/

/*** prototypes {{{ ***/

void editorSetStatusMessage(const char *fmt, ...);
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

void putchar_txt(char c) __naked {
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

void gotoxy_(char x, char y) __naked {
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

void gotoxy(int x, int y) {
  cursor_pos.x = x;
  cursor_pos.y = y;
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
  initxt(80);
  perror(s);
  quit_program(1);
}

int getCursorPosition(int *rows, int *cols) {
  *rows = cursor_pos.y;
  *cols = cursor_pos.x;
  return 0;
}

int getWindowSize(int *rows, int *cols) {
  *rows = 26;
  *cols = 85;
  return 0;
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

void screen(char mode) __naked {
  mode;
  __asm
    push ix
    ld ix,#4
    add ix,sp

    ld a,0(ix)
    ld ix,CHGMOD
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

void fillvram(char data, int length, int address) __naked {
  data;
  length;
  address;
  __asm
    push ix
    ld ix,#4
    add ix,sp

    ld a,0(ix)
    ld c,1(ix)
    ld b,2(ix)
    ld l,3(ix)
    ld h,4(ix)
    ld ix,FILVRM
    BIOSCALL

    pop ix
    ret
  __endasm;
}

void disable_screen(void) __naked {
  __asm
    push ix
    ld ix,DISSCR
    BIOSCALL
    pop ix
    ret
  __endasm;
}

void enable_screen(void) __naked {
  __asm
    push ix
    ld ix,ENASCR
    BIOSCALL
    pop ix
    ret
  __endasm;
}


void putchar(char c) {
  MMMtask vdptask;
  unsigned char n,m;


  if (escape_sequence == 1) {
    // If previous char was an escape sequence \33
    if (c == 'K') {
      // Delete everything from the cursor position to the end of line
      escape_sequence = 0;

      vdptask.X2 = cursor_pos.x*CHAR_SIZEX*2;
      vdptask.Y2 = cursor_pos.y*CHAR_SIZEY;
      vdptask.DX = SCREEN7_SIZEX - cursor_pos.x*CHAR_SIZEX*2;
      vdptask.DY = CHAR_SIZEY;
      vdptask.s0 = 0x11;
      vdptask.DI = 0;
      vdptask.LOP = opHMMV;
      fLMMM(&vdptask);

      cursor_pos.x = E.screencols;
    } else if (c == 'x') {
      // Cursor
      // TODO
    } else if (c == 'm') {
      // Color
      // Based on ANSI but different
      // Colors from 0 to 15 (+32)
      // inverted 16+32
      escape_sequence_2 = 1;
    } else if (escape_sequence_2 == 1) {
      if (c-32 <= 15) {
        fgcolor = c-32;
      } else {
        if (inverted == 0) {
          inverted = 1;
        } else {
          inverted = 0;
        }
      }
      escape_sequence_2 = 0;
      escape_sequence = 0;
    } else if (c == 'y') {
      // Cursor
      // TODO
    } else if (c == 'H') {
      // Move to top left corner
      gotoxy(0, 0);
      escape_sequence = 0;
    } else if (c == 'Y') {
      // Move cursor to xy position
      escape_sequence_2 = 2;
      escape_sequence_y = 127;
    } else if (escape_sequence_2 == 2) {
      if (escape_sequence_y == 127) {
        escape_sequence_y = c;
      } else {
        gotoxy(c-32, escape_sequence_y-32);
        escape_sequence_2 = 0;
        escape_sequence = 0;
        escape_sequence_y = 0;
      }

    } else {
      escape_sequence = 0;
    }
  } else {
    // If previous char wasn't an escape sequence \33
    if (c >= 0x20) {
      n = c%BACKBUFFER_LENGTH;
      m = c/BACKBUFFER_LENGTH;

      // Set background
      vdptask.X2 = cursor_pos.x*CHAR_SIZEX*2;
      vdptask.Y2 = cursor_pos.y*CHAR_SIZEY;
      vdptask.DX = CHAR_SIZEX*2;
      vdptask.DY = CHAR_SIZEY;
      vdptask.s0 = (fgcolor<<4) + fgcolor;
      vdptask.DI = 0;
      vdptask.LOP = opHMMV;
      fLMMM(&vdptask);

      // Set char
      vdptask.X = n*CHAR_SIZEX*2;
      vdptask.Y = 212+m*CHAR_SIZEY;
      vdptask.DX = CHAR_SIZEX*2;
      vdptask.DY = CHAR_SIZEY;
      vdptask.X2 = cursor_pos.x*CHAR_SIZEX*2;
      vdptask.Y2 = cursor_pos.y*CHAR_SIZEY;
      vdptask.s0 = 0;
      vdptask.DI = 0;
      if (inverted == 0) {
        vdptask.LOP = LOGICAL_AND;
      } else {
        vdptask.LOP = LOGICAL_XOR;
      }
      fLMMM(&vdptask);

      cursor_pos.x++;
    } else if (c == '\n') {
      cursor_pos.y++;
    } else if (c == '\r') {
      cursor_pos.x = 0;
    } else if (c == '\33') {
      escape_sequence = 1;
    }
  }
}

void vputchar_vram(unsigned char c, unsigned int addr) {
  int x,y;
  unsigned char b, p1, p2, p3;

  for (y=0; y<CHAR_SIZEY; y++) {
    b = char_table[c*CHAR_SIZEY + y];
    for (x=0; x<CHAR_SIZEX; x++) {
      p1 = ((b >> (CHAR_SIZEX*2)-x*2+1) & 1U) * fgcolor;
      p2 = ((b >> (CHAR_SIZEX*2)-x*2) & 1U) * fgcolor;
      if (p1 == 0) p1 = bgcolor;
      if (p2 == 0) p2 = bgcolor;
      p3 = (p1 << 4) + p2;
      vpoke(addr + x + y*SCREEN7_SIZEX/2 ,p3);
    }
  }
}

void vcolorprint(char* str) {
  int x=0;

  while (str[x] != '\0') {
    putchar(str[x]);
    gotoxy(cursor_pos.x++,cursor_pos.y);
    x++;
  }
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

void editorAppendRow(char *s, size_t len) {
  int at;

  // TODO Fix crash when load file
  // it looks like it has to do with bad malloc and realloc implementation
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
    editorAppendRow("", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
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
  //int size_read;
  
  free(E.filename);
  E.filename = strdup(filename);

  fp  = open(filename, O_RDONLY);

  if (fp < 0) die("Error opening file");
  while (fgets(buffer, FILE_BUFFER_LENGTH, fp) != NULL) {
    editorAppendRow(buffer, strlen(buffer));
  }

  close(fp);
  E.dirty = 0;
}

void editorSave() {
  int len;
  char *buf;
  int fd;
  //int ret;

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

        sprintf(welcome, "\33m%cMSX\33m%c vi -- version %s",LIGHT_BLUE+32 , WHITE+32, MSXVI_VERSION);
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

  abAppend(ab, "\33m0", 3); // Inverted text
  
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
  abAppend(ab, "\33m0", 3); // Inverted text
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
  int msglen;

  abAppend(ab, "\33K\r", 3);
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
      // TODO
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
  int n, y, x;

  // Check MSX-DOS version >= 2
  if(dosver() < 2) {
    die("This program requires MSX-DOS 2 to run.");
  }

  Save_VDP();		// Save VDP internals
  // Get the char table
  initxt(80);
  for (n = 0; n < 8*255; n++) {
    char_table[n] = vpeek(CHARTABLE + n);
  }

  // Set graphic mode
  color(WHITE, BLACK, BLACK);
  screen(7);
  SetFasterVDP();	// optimize VDP, sprites off
  SetPage(0);		// Set the main page 0
  SetBorderColor(0);	// Background + border
  // SetPalette((Palette *)palette);

  // Set base colors
  fgcolor = WHITE;
  bgcolor = BLACK;
  inverted = 0;

  // Put all chars in backbufffer
  y = -1;
  for (n = 0; n < 255; n++) {
    if (n%BACKBUFFER_LENGTH == 0) {
      y++;
      x=0;
    }
    vputchar_vram(n, SCREEN7_BACKBUF + x*CHAR_SIZEX + y*SCREEN7_SIZEX/2*CHAR_SIZEY);
    x++;
  }

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
