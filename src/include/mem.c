

#include "types.h"
#include "mem.h"


void memcpy(uint8_t *dest, uint8_t *src, uint16_t n) {
	while (n > 0) {
		*dest = *src;
		dest++;
		src++;
		n--;
	}
}


void memset(uint8_t *s, uint8_t c, uint16_t n) {
	while (n > 0) {
		*s = c;
		s++;
		n--;
	}
}


