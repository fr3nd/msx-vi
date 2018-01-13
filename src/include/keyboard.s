

	.area _CODE


_keyboard_read::
	push ix
	ld ix,#0
	add ix,sp

	in a,(#0xAA)
	and #0xF0       ; only change bits 0-3
	or #8           ; row 8
	out (#0xAA),a
	in a,(#0xA9)    ; read row into A
	xor #0xFF
	ld l,a

	pop ix
	ret

