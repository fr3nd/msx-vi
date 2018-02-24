HEX2BINDIR = ../utils/Hex2bin-1.0.12
HEX2BIN = $(HEX2BINDIR)/hex2bin
.PHONY: clean libs libsclean
C_FILES := $(wildcard *.c)
IHX_FILES := $(subst .c,.ihx,$(C_FILES))
COM_FILES := $(subst .ihx,.com,$(IHX_FILES))
AS = sdasz80
CC = sdcc
INCLUDEDIR = include

# Uncomment the CCFLAGS variable depending on your needs
#CCFLAGS = --code-loc 0x0178 --data-loc 0 --disable-warning 196 -mz80 --no-std-crt0 -I$(INCLUDEDIR) include/crt0msx_msxdos_advanced.rel include/putchar.rel include/getchar.rel include/dos2.rel
#CCFLAGS = --code-loc 0x0107 --data-loc 0 --disable-warning 196 -mz80 --no-std-crt0 -I$(INCLUDEDIR) include/crt0msx_msxdos.rel
CCFLAGS := --code-loc 0x0178 --data-loc 0 --disable-warning 196 -mz80 --no-std-crt0 -I$(INCLUDEDIR) $(INCLUDEDIR)/crt0msx_msxdos_advanced.rel include/heap.rel

all: hex2bin libs $(COM_FILES)

hex2bin:
	$(MAKE) -C $(HEX2BINDIR)

libs:
	$(MAKE) -C $(INCLUDEDIR)

libsclean:
	$(MAKE) -C $(INCLUDEDIR) clean

%.ihx: %.c
	$(CC) $(CCFLAGS) $^

%.com: %.ihx
	mkdir -p bin
	$(HEX2BIN) -e com $^
	cp $@ bin/

clean: libsclean
	find *.rel -type f -not -name 'VDP*' -delete || true
	find dsk \
		-not -name 'COMMAND2.COM' \
		-not -name 'MSXDOS2.SYS' \
		-not -name 'AUTOEXEC.BAT' \
		-delete || true
	rm -f *.com *.asm *.lst *.sym *.bin *.ihx *.lk *.map *.noi
	rm -rf bin/
	rm -f vi.zip

test: release
	openmsx -machine msx2 -ext msxdos2 -diska dsk/ -script ../emulation/boot.tcl

release: all
	cp bin/vi.com dsk/
	cp -r ../tests/*.txt dsk/
	7z a vi.zip ./dsk/*

# vim:ft=make
#
