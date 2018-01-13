

	.area _CODE


_putchar::       
	ld      hl,#2
	add     hl,sp
        
	;ld      l,(hl)
	;ld      a,#1
	;rst     0x08

	ld		e,(hl)
	ld		c,#2
	call	5
        
	ret
