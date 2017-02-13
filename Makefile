CC      = avr-gcc
OBJCOPY = avr-objcopy
RM      = rm -f
AVRDUDE = avrdude

INCLUDEDIR = ../libs/include
LIBDIR     = ../libs/lib

MCU     = atmega328p
F_CPU   = 16000000UL
CFLAGS  = -Wall -Os -DF_CPU=$(F_CPU) -mmcu=$(MCU) -I$(INCLUDEDIR)
LDFLAGS = -L$(LIBDIR)

BIN_FORMAT = ihex
PROTOCOL   = arduino -P /dev/ttyUSB0 #-b57600

LDLIBS = -lserial -lnrf24l01+

.PHONY: all
all: main.hex

main.hex: main.elf

main.elf: main.s

main.s: main.c

.PHONY: clean
clean:
	$(RM) main.elf main.hex main.s

.PHONY: upload
upload: main.hex
	$(AVRDUDE) -c $(PROTOCOL) -p $(MCU) -U flash:w:$<

%.elf: %.s ; $(CC) $(CFLAGS) -s -o $@ $< $(LDFLAGS) $(LDLIBS)

%.s: %.c ; $(CC) $(CFLAGS) -S -o $@ $< $(LDFLAGS) $(LDLIBS)

%.hex: %.elf ; $(OBJCOPY) -O $(BIN_FORMAT) -R .eeprom $< $@
