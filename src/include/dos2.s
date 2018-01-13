

	.area _DATA


_last_error::
	.ds 1


	.area _CODE


_open::
	push ix
	ld ix,#0
	add ix,sp

	; path
	ld e,4(ix)
	ld d,5(ix)
	; flags
	ld a,6(ix)
	; call
	ld c,#0x43
	call 5
	; check error
	ld (_last_error),a
	add a,#0
	jp z,_open_ok
	ld l,#-1
	jp _open_end
_open_ok:
	ld l,b
_open_end:

	pop ix
	ret



_creat::
	push ix
	ld ix,#0
	add ix,sp

	; path
	ld e,4(ix)
	ld d,5(ix)
	; flags
	ld a,6(ix)
	; attrib
	ld b,7(ix)
	; call
	ld c,#0x44
	call 5
	; check error
	ld (_last_error),a
	add a,#0
	jp z,_creat_ok
	ld l,#-1
	jp _creat_end
_creat_ok:
	ld l,b
_creat_end:

	pop ix
	ret



_close::
	push ix
	ld ix,#0
	add ix,sp

	; handle
	ld b,4(ix)
	; call
	ld c,#0x45
	call 5
	; return
	ld (_last_error),a
	ld l,a

	pop ix
	ret



_dup::
	push ix
	ld ix,#0
	add ix,sp

	; handle
	ld b,4(ix)
	; call
	ld c,#0x47
	call 5
	; return
	ld (_last_error),a
	add a,#0
	jp z,_dup_ok
	ld l,#-1
	jp _dup_end
_dup_ok:
	ld l,b
_dup_end:

	pop ix
	ret	



_read::
	push ix
	ld ix,#0
	add ix,sp

	; handle
	ld b,4(ix)
	; buffer
	ld e,5(ix)
	ld d,6(ix)
	; bytes
	ld l,7(ix)
	ld h,8(ix)
	; call
	ld c,#0x48
	call 5
	; return
	ld (_last_error),a
	add a,#0
	jp z,_read_end
	ld h,#-1
	ld l,#-1
_read_end:

	pop ix
	ret



_write::
	push ix
	ld ix,#0
	add ix,sp

	; handle
	ld b,4(ix)
	; buffer
	ld e,5(ix)
	ld d,6(ix)
	; bytes
	ld l,7(ix)
	ld h,8(ix)
	; call
	ld c,#0x49
	call 5
	; return
	ld (_last_error),a
	add a,#0
	jp z,_write_end
	ld h,#-1
	ld l,#-1
_write_end:

	pop ix
	ret



_lseek::
	push ix
	ld ix,#0
	add ix,sp

	; handle
	ld b,4(ix)
	; offset (32bit)
	ld l,5(ix)
	ld h,6(ix)
	ld e,7(ix)
	ld d,8(ix)
	; method
	ld a,9(ix)
	; call
	ld c,#0x4A
	call 5
	; return
	ld (_last_error),a
	add a,#0
	jp z,_lseek_end
	ld h,#-1
	ld l,#-1
_lseek_end:

	pop ix
	ret



_exit::
	push ix
	ld ix,#0
	add ix,sp

	ld b,4(ix)
	ld c,#0x62
	call 5

	pop ix
	ret


