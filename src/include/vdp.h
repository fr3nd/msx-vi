

#ifndef  __VDP_H__
#define  __VDP_H__


#include "types.h"


struct vdp_copy_command {
	union word_byte source_x;
	union word_byte source_y;
	union word_byte dest_x;
	union word_byte dest_y;
	union word_byte size_x;
	union word_byte size_y;
	uint8_t data;
	uint8_t argument;
	uint8_t command;
};


extern void vdp_set_screen5(void);
extern void vdp_set_text(void);
extern void vdp_load_palette(void *p);
extern void vdp_set_write_address(uint8_t h, uint16_t l);
extern void vdp_set_read_address(uint8_t h, uint16_t l);
extern void vdp_load_screen(uint8_t *source, uint8_t source_size);
extern void vdp_copy(struct vdp_copy_command *c);


#endif  // __VDP_H__
