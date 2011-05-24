CC=/usr/bin/avr-gcc
CFLAGS=-std=c99 -g -Os -Wall -mcall-prologues -mmcu=atmega88 -I/usr/avr/include -Wl,-L/usr/lib64/binutils/avr/2.19.1 -DF_CPU=16000000UL
OBJ2HEX=/usr/bin/avr-objcopy
UISP=/usr/bin/uisp
AVRPORT=/dev/ttyUSB0
AVRPART=m88
PROGSPEED=57600
PROGRAMMER=usbtiny
AVRDUDE=/home/niko/arduino-0017/hardware/tools/avrdude
AVRDUDECONF=/home/niko/arduino-0017/hardware/tools/avrdude.conf

TARGET=main
INCLUDES=

all: build program

build: $(TARGET).hex

.PHONY: program
program : $(TARGET).hex
	$(AVRDUDE) -C$(AVRDUDECONF) -p$(AVRPART) -c$(PROGRAMMER) -v -U flash:w:$^

#	$(UISP) -dprog=stk500 -dserial=$(AVRPORT) --erase -dpart=atmega32
#	$(UISP) -dprog=stk500 -dserial=$(AVRPORT) --upload -dpart=atmega32 \
		if=$(TARGET).hex -v=2
#	$(UISP) -dprog=stk500 -dserial=$(AVRPORT) --verify -dpart=atmega32 \
		if=$(TARGET).hex -v=2

${TARGET}.o: ${TARGET}.c ${INCLUDES}

%.obj : %.o
	$(CC) $(CFLAGS) $< -o $@

%.hex : %.obj
	$(OBJ2HEX) -R .eeprom -O ihex $< $@

clean:
	rm -f *.hex *.obj *.o *~
