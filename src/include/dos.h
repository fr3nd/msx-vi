

#ifndef  __DOS_H__
#define  __DOS_H__


#ifdef  __DOS2_H__
#error You cannot use both dos and dos2 file functions
#endif


#include "types.h"


typedef struct {
	uint8_t drive;        // 0: default drive
	char name[11];
	uint8_t reserved1[3];
	uint8_t record_size;
	uint32_t size;
	uint8_t reserved2[26];
} fcb;


extern int8_t last_error;


extern int8_t open(fcb *);
extern int8_t creat(fcb *);
extern int8_t close(fcb *);
extern int8_t block_set_data_ptr(void *);
extern int8_t block_read(fcb *);
extern int8_t block_write(fcb *);
extern void exit(int8_t);


#endif
