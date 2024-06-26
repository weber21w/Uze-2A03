###############################################################################
# Makefile for the project Uze-2A03
###############################################################################

## General Flags
PROJECT = Uze-2A03
GAME= Uze-2A03
MCU = atmega644
TARGET = $(GAME).elf
CC = avr-gcc
INFO=gameinfo.properties

## Kernel settings
KERNEL_DIR = ../../../pcm-kernel
KERNEL_OPTIONS  = -DVIDEO_MODE=3 -DINTRO_LOGO=0 -DINCLUDE_DEFAULT_WAVES=0 -DSOUND_MIXER=2 -DENABLE_MIXER=0 -DSOUND_PLAYER_DISABLE=1
KERNEL_OPTIONS += -DMAX_SPRITES=16 -DRAM_TILES_COUNT=30 -DSCREEN_TILES_V=22 -DFIRST_RENDER_LINE=53 -DTRANSLUCENT_COLOR=0x85
KERNEL_OPTIONS += -DRESOLUTION_EXT=1 -DVRAM_TILES_H=32 -DTRUE_RANDOM_GEN=1 #-DSPRITES_VSYNC_PROCESS=0

UZEBIN_DIR = ../../../bin
## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -Wall -gdwarf-2 -std=gnu99 -DF_CPU=28636360UL -Os -fsigned-char -ffunction-sections -fno-toplevel-reorder
CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d 
CFLAGS += $(KERNEL_OPTIONS)


## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS += -Wl,-Map=$(GAME).map 
LDFLAGS += -Wl,-gc-sections 


## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings


## Objects that must be built in order to link
OBJECTS = uzeboxVideoEngineCore.o uzeboxCore.o uzeboxSoundEngine.o uzeboxSoundEngineCore.o uzeboxVideoEngine.o sdBase.o spiram.o U2A03.o $(GAME).o 

## Objects explicitly added by the user
LINKONLYOBJECTS = 

## Include Directories
INCLUDES = -I"$(KERNEL_DIR)" 

## Build
##all: $(TARGET) $(GAME).hex $(GAME).eep $(GAME).lss $(GAME).uze size
all: VGMNES01.DAT $(TARGET) $(GAME).hex $(GAME).eep $(GAME).lss $(GAME).uze size

## Rebuild graphics resources
../data/tiles.inc: ../data/tiles.png ../data/tiles.xml
	$(UZEBIN_DIR)/gconvert ../data/tiles.xml

../data/sprites.inc: ../data/sprites.png ../data/sprites.xml
	$(UZEBIN_DIR)/gconvert ../data/sprites.xml



## Compile Kernel files
uzeboxVideoEngineCore.o: $(KERNEL_DIR)/uzeboxVideoEngineCore.s
	$(CC) $(INCLUDES) $(ASMFLAGS) -c  $<

uzeboxSoundEngineCore.o: $(KERNEL_DIR)/uzeboxSoundEngineCore.s
	$(CC) $(INCLUDES) $(ASMFLAGS) -c  $<

uzeboxCore.o: $(KERNEL_DIR)/uzeboxCore.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

uzeboxSoundEngine.o: $(KERNEL_DIR)/uzeboxSoundEngine.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

uzeboxVideoEngine.o: $(KERNEL_DIR)/uzeboxVideoEngine.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

U2A03.o: ../U2A03.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

## Compile game sources
sdBase.o: $(KERNEL_DIR)/sdBase.S
	$(CC) $(INCLUDES) $(ASMFLAGS) -c  $<

spiram.o: $(KERNEL_DIR)/spiram.s
	$(CC) $(INCLUDES) $(ASMFLAGS) -c  $<

#bootlib.o: $(KERNEL_DIR)/bootlib.s
#	$(CC) $(INCLUDES) $(ASMFLAGS) -c  $<

$(GAME).o: ../$(GAME).c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<
##Link
$(TARGET): $(OBJECTS)
	 $(CC) $(LDFLAGS) $(OBJECTS) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $(TARGET)

%.hex: $(TARGET)
	avr-objcopy -O ihex $(HEX_FLASH_FLAGS)  $< $@

%.eep: $(TARGET)
	-avr-objcopy $(HEX_EEPROM_FLAGS) -O ihex $< $@ || exit 0

%.lss: $(TARGET)
	avr-objdump -h -S $< > $@

%.uze: $(TARGET)
	$(UZEBIN_DIR)/packrom $(GAME).hex $@ $(INFO)

##UNAME := $(shell sh -c 'uname -s 2>/dev/null || echo not')
AVRSIZEFLAGS := -A ${TARGET}
##ifneq (,$(findstring MINGW,$(UNAME)))
##AVRSIZEFLAGS := -C --mcu=${MCU} ${TARGET}
##endif

size: ${TARGET}
	@echo
	@avr-size -C --mcu=${MCU} ${TARGET}

## Clean target
.PHONY: clean
clean:
	-rm -rf SD_MUSIC.DAT $(OBJECTS) $(GAME).* dep/* *.uze


## Other dependencies
-include $(shell mkdir dep 2>/dev/null) $(wildcard dep/*)

