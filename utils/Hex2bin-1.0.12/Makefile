# Makefile hex2bin
WIN_GCC = amd64-mingw32msvc-gcc

INSTALL_DIR = /usr/local
MAN_DIR = $(INSTALL_DIR)/man/man1

all: hex2bin mot2bin hex2bin.1

hex2bin.1: hex2bin.pod
	pod2man hex2bin.pod > hex2bin.1

hex2bin: hex2bin.c
	gcc -O2 -Wall -o hex2bin hex2bin.c

mot2bin: mot2bin.c
	gcc -O2 -Wall -o mot2bin mot2bin.c

windows:
	$(WIN_GCC) -O2 -Wall -o Windows/hex2bin.exe hex2bin.c
	$(WIN_GCC) -O2 -Wall -o Windows/mot2bin.exe mot2bin.c

install:
	cp hex2bin mot2bin $(INSTALL_DIR)/bin
	cp hex2bin.1 $(MAN_DIR)

clean:
	rm -f core *.o hex2bin mot2bin
