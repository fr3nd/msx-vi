

#ifndef  __KEYBOARD_H__
#define  __KEYBOARD_H__


#include "types.h"


#define  KEYBOARD_RIGHT  0x80
#define  KEYBOARD_DOWN   0x40
#define  KEYBOARD_UP     0x20
#define  KEYBOARD_LEFT   0x10
#define  KEYBOARD_DEL    0x08
#define  KEYBOARD_INS    0x04
#define  KEYBOARD_HOME   0x02
#define  KEYBOARD_SPACE  0x01


extern uint8_t keyboard_read(void);


#endif  // __KEYBOARD_H__
