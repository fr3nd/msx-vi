
#ifndef  __DOS2_H__
#define  __DOS2_H__

#ifdef  __DOS_H__
#error You cannot use both dos and dos2 file functions
#endif

#include "types.h"

/* standard descriptors */
#define  STDIN   0
#define  STDOUT  1
#define  STDERR  2
#define  AUX     3
#define  PRN     4

/* open/creat flags */
#define  O_RDONLY   0x01
#define  O_WRONLY   0x02
#define  O_RDWR     0x00
#define  O_INHERIT  0x04

/* creat attributes */
#define  ATTR_RDONLY  0x01
#define  ATTR_HIDDEN  0x02
#define  ATTR_SYSTEM  0x04
#define  ATTR_VOLUME  0x08
#define  ATTR_FOLDER  0x10
#define  ATTR_ARCHIV  0x20
#define  ATTR_DEVICE  0x80

/* seek whence */
#define  SEEK_SET  0
#define  SEEK_CUR  1
#define  SEEK_END  2


extern uint8_t last_error;


extern int8_t open(char *, uint8_t);
extern int8_t creat(char *, uint8_t, uint8_t);
extern int8_t close(int8_t);
extern int8_t dup(int8_t);
extern int16_t read(int8_t, void *, int16_t);
extern int16_t write(int8_t, void *, int16_t);
extern uint32_t lseek(int8_t, uint32_t, uint8_t);
extern void exit(int8_t);


#endif
