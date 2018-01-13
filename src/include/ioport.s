

	.area _CODE


_in::
	push ix
	ld ix,#0
	add ix,sp

	ld c,4(ix)
	in l,(c)

	pop ix
	ret


_out::
	push ix
	ld ix,#0
	add ix,sp

	ld c,4(ix)
	ld a,5(ix)
	out (c),a

	pop ix
	ret

