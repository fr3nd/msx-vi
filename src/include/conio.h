

#ifndef  __CONIO_H__
#define  __CONIO_H__


#include "types.h"


extern void putchar(char c);
extern char getchar(void);
extern void puts(char *s);
extern void puthex8(uint8_t v);
extern void puthex16(uint16_t v);
extern void putdec8(uint8_t v);
extern void putdec16(uint16_t v);


#endif  // __CONIO_H__
