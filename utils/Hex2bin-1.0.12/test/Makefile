test:
# Generates a checksum error, but 'make' will ignore this error ('-' before hex2bin)
	-hex2bin -c nochecksum.hex
	hex2bin -c linear.hex
	hex2bin -c segmented.hex
	hex2bin -c example.hex
	mot2bin -c example2.s19
	hex2bin -c test-end-memory.hex
	hex2bin -c test-overlap.hex
	mot2bin -c test-addr_mask.s19
	hex2bin -c test.hex
	hex2bin -c test-extra-lf.hex

pad:
	hex2bin -c -p 3A example.hex
	hex2bin -c -l 1000 -e bin2 example.hex
	hex2bin -c -p 3A -l 1000 -e bin3 example.hex
	hex2bin -c -p 3A -l 2000 -e bin4 example.hex

options:
	-hex2bin < nul xx
	-hex2bin -c
	-hex2bin -h
	-hex2bin -?
	-hex2bin -h example.hex
	-hex2bin -? example.hex
	-hex2bin -ecmd example.hex
	hex2bin -e cmd example.hex
	-hex2bin -e.cmd example.hex
	hex2bin -e .cmd example.hex
	-hex2bin -c -p3A test.hex
	hex2bin -w test-byte-swap.hex

cksrange:
	hex2bin -s 0000 -k 0 -r 01C0 01C8 -f 01C9 00 -c example.hex

start:
	hex2bin -c -s 0000 -e bin6 example.hex

maxlength:
	hex2bin -p 3A -l800000 -e bin5 example.hex

clean:
	rm *.bin* *.cmd *..cmd
