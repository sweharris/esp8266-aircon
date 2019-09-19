PORT=/dev/ttyUSB0
BOARD=esp8266:esp8266:nodemcuv2

SRC = $(wildcard *.ino)

dummy: $(SRC)
	@mkdir -p tmp
	@TMPDIR=$(PWD)/tmp arduino-cli compile --fqbn=$(BOARD) --build-path=$(PWD)/build --build-cache-path=$(PWD)/cache
	@rm -rf tmp
	@touch dummy

upload:
	@mkdir -p tmp
	@TMPDIR=$(PWD)/tmp arduino-cli upload --fqbn=$(BOARD) -p $(PORT)
	@rm -rf tmp

serial:
	@kermit -l $(PORT) -b 115200 -c

clean:
	rm -rf *.elf build cache tmp dummy

realclean: clean
	rm -rf *.bin
