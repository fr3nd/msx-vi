all:
	$(MAKE) -C src

clean:
	$(MAKE) -C src clean
	$(MAKE) -C utils/Hex2bin-1.0.12 clean

hex2bin:
	$(MAKE) -C utils/Hex2bin-1.0.12

test:
	$(MAKE) -C src test

release:
	$(MAKE) -C src release

# vim:ft=make
#
