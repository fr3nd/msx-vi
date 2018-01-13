

	.area _CODE


_vdp_set_screen5::
	push ix

	; ponemos el modo screen 5
	ld iy,(#0xFCC0)
	ld ix,#0x005F
	ld a,#5
	call #0x001C
	; visualizamos la pagina 0
	ld a,#0x1F
	out (#0x99),a
	ld a,#0x82
	out (#0x99),a
	; ponemos el 0 como color de fondo
	xor a
	out (#0x99),a
	ld a,#0x87
	out (#0x99),a
	; activamos las interrupciones vblank
	ld a,#0x62
	out (#0x99),a
	ld a,#0x81
	out (#0x99),a

	pop ix
	ret


_vdp_set_text::
	push ix

	ld iy,(#0xFCC0)
	ld ix,#0x5F
	ld a,#0
	call #0x001C

	pop ix
	ret


_vdp_load_palette::
	push ix
	ld ix,#0
	add ix,sp

	ld l,4(ix)
	ld h,5(ix)
	xor	a             ; Set p#pointer to zero.
	di
	out	(#0x99),a
	ld	a,#0x90
	ei
	out	(#0x99),a
	ld	bc,#0x209A    ; out 32x to port #9A
	otir

	; iteramos por cada color
	;;;ld b,#32
	; seleccionamos el color del registro B en la paleta
	; ponemos las componentes tal y como vienen en el fichero
;;;_vdp_load_palette__fill:
	;;;ld a,(hl)
	;;;inc hl
	;;;out (#0x9A),a
	;;;djnz _vdp_load_palette__fill
;;;_vdp_load_palette__end:

	pop ix
	ret


_vdp_set_write_address::
	push ix
	ld ix,#0
	add ix,sp

	ld a,4(ix)
	ld l,5(ix)
	ld h,6(ix)
	rlc h
	rla
	rlc h
	rla
	srl h
	srl h
	di
	out (#0x99),a
	ld a,#14+#128
	out (#0x99),a
	ld a,l
	nop
	out (0x99),a
	ld a,h
	or #64
	ei
	out	(0x99),a

	pop ix
	ret


_vdp_set_read_address::
	push ix
	ld ix,#0
	add ix,sp

	ld a,4(ix)
	ld l,5(ix)
	ld h,6(ix)
	rlc h
	rla
	rlc h
	rla
	srl h
	srl h
	di
	out (#0x99),a
	ld a,#14+#128
	out (#0x99),a
	ld a,l
	nop
	out (#0x99),a
	ld a,h
	ei
	out (#0x99),a

	pop ix
	ret


_vdp_load_screen::
	push ix
	ld ix,#0
	add ix,sp

	ld l,4(ix)
	ld h,5(ix)
	ld b,6(ix)
_vdp_load_screen__fill:
	ld a,(hl)
	inc hl
	out (#0x98),a
	djnz _vdp_load_screen__fill

	pop ix
	ret



_vdp_copy::
	push ix
	ld ix,#0
	add ix,sp

_vdp_copy__vdp_ready:
	ld a,#2
	di
	out (#0x99),a		;select s#2
	ld a,#15+#128
	out (#0x99),a
	in a,(#0x99)
	rra
	ld a,#0		;back to s#0, enable ints
	out (#0x99),a
	ld a,#15+#128
	out (#0x99),a		;loop if vdp not ready (CE)
	ei
	jp c,_vdp_copy__vdp_ready
	ld l,4(ix)
	ld h,5(ix)
	ld a,#32
	di
	out (#0x99),a
	ld a,#17+#128
	out (#0x99),a
	ld b,#0
	ld c,#0x9B
	outi
	outi
	outi
	outi
	outi
	outi
	outi
	outi
	outi
	outi
	outi
	outi
	outi
	outi
	outi
	ei

	pop ix
	ret


_aux_vdp_copy::
	push ix
	ld ix,#0
	add ix,sp

	ld l,4(ix)
	ld h,5(ix)
	ld a,#32
	di
	out (#0x99),a
	ld a,#17+#128
	out (#0x99),a
	ld c,#0x9B
_aux_vdp_copy__vdp_ready:
	ld a,#2
	di
	out (#0x99),a		;select s#2
	ld a,#15+#128
	out (#0x99),a
	in a,(#0x99)
	rra
	ld a,#0		;back to s#0, enable ints
	out (#0x99),a
	ld a,#15+#128
	ei
	out (#0x99),a		;loop if vdp not ready (CE)
	jp c,_aux_vdp_copy__vdp_ready
	outi
	outi
	outi
	outi
	outi
	outi
	outi
	outi
	outi
	outi
	outi
	outi
	outi
	outi
	outi

	pop ix
	ret

