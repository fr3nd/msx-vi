#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

void enableRawMode() {
}

void disableRawMode() {
}

int main() {
  char c;

  enableRawMode();

  do {
    c = getchar();
    if (iscntrl(c)) {
      printf("%d\n\r", c);
    } else {
      printf("%d ('%c')\n\r", c, c);
    }
  } while (c != 'q');

  disableRawMode();
  return 0;
}
