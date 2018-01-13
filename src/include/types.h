

#ifndef  __TYPES_H__
#define  __TYPES_H__


typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed int int16_t;
typedef unsigned int uint16_t;
typedef signed long int32_t;
typedef unsigned long uint32_t;
typedef float float32;

union word_byte {
	uint16_t w;
	struct {
		uint8_t lsb;
		uint8_t msb;
	} b;
};

#ifndef  NULL
#define  NULL  ((void *) 0)
#endif


#endif  // __TYPES_H__
