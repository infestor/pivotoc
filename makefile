## General Flags
PROJECT = pivotoc
MCU = atmega328p
PRIPONA = .elf
TARGET = pivotoc.elf

#CESTA=/usr/local/CrossPack-AVR/avr/bin/
#CESTA="C:/WinAVR-20100110\\bin\\"
CESTA =

CC = $(CESTA)avr-c++
CPP = $(CESTA)avr-c++

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -ffreestanding
CFLAGS += -fno-tree-scev-cprop
CFLAGS += -mcall-prologues 
CFLAGS += -fno-jump-tables
#CFLAGS += -std=gnu99
CFLAGS += -Wall -gdwarf-2 -g -DF_CPU=16000000UL -O1 
CFLAGS += -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
CFLAGS += -fdata-sections -ffunction-sections
#CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d 

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON) -Wl,--gc-sections 
LDFLAGS +=  -Wl,-Map=pivotoc.map

## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom -R .fuse -R .lock -R .signature

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings

## Include Directories
#INCLUDES = -I"C:/WinAVR-20100110/avr/include" -I"C:/Honza/_nrf24l01"
#INCLUDES = -I"/usr/local/CrossPack-AVR/avr/include/"
INCLUDES = 

## Library Directories
#LIBDIRS = -L"C:/WinAVR-20100110/avr/lib" 
#LIBDIRS = -L"/usr/local/CrossPack-AVR/avr/lib/"
LIBDIRS = 

## Objects that must be built in order to link
OBJECTS = pivotoc_main.o onewire.o

## Objects explicitly added by the user
LIBS = -lprintf_flt -lm

## Build
all: clean $(TARGET) pivotoc.hex pivotoc.eep pivotoc.lss avr-size
	
touch:	
	touch pivotoc_main.cpp
	
## Compile
%.o: %.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

%.o: %.cpp
	$(CPP) $(INCLUDES) $(CFLAGS) -c  $<

##Link
$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $(TARGET)
    
%.hex: $(TARGET)
	$(CESTA)avr-objcopy -O ihex $(HEX_FLASH_FLAGS)  $< $@

%.eep: $(TARGET)
	$(CESTA)avr-objcopy $(HEX_EEPROM_FLAGS) -O ihex $< $@ || exit 0

%.lss: $(TARGET)
	$(CESTA)avr-objdump -h -S $< > $@

avr-size:
	$(CESTA)avr-size --mcu=$(MCU) --format=avr $(TARGET)

## Clean target
.PHONY: clean
clean:
	-rm -rf $(OBJECTS) pivotoc.elf dep/* pivotoc.hex pivotoc.eep pivotoc.lss pivotoc.map


## Other dependencies
#-include $(shell mkdir dep 2>NUL) $(wildcard dep/*)


