ASM_FILES := $(wildcard *.s)
REL_FILES := $(subst .s,.rel,$(ASM_FILES))
C_FILES := $(wildcard *.c)
REL2_FILES := $(subst .c,.rel,$(C_FILES))
HEX2BIN = ../../utils/Hex2bin-1.0.12/hex2bin
AS = sdasz80
CC = sdcc

all: $(REL_FILES) $(REL2_FILES)

%.rel: %.s
	$(AS) -o $^

%.rel: %.c
	$(CC) -mz80 -c -o $@ $^

clean:
	find *.rel -type f -not -name 'VDP*' -delete || true
	rm -f *.asm *.lst *.sym

# vim:ft=make
#
