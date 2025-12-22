TARGET = hello
include $(KOS_BASE)/Makefile.rules

GCCGO = sh-elf-gccgo -O2 -ml -m4-single -fno-split-stack -I$(KOS_BASE)/lib -L$(KOS_BASE)/lib
OUTPUT ?= $(TARGET).elf

# Auto-detect romdisk directory
ROMDISK_DIR = romdisk
HAS_ROMDISK = $(wildcard $(ROMDISK_DIR)/*)
ROMDISK_OBJ = $(if $(HAS_ROMDISK),romdisk.o,)

# Library search paths
LIB_PATHS = -L$(KOS_BASE)/lib -L$(KOS_BASE)/addons/lib/dreamcast -L$(KOS_BASE)/../kos-ports/lib

# Core libraries (always needed)
# Note: libparallax requires all image loaders due to internal dependencies
# Audio streaming libraries: libwav (WAV), libtremor (OGG Vorbis)
LIBS = -Wl,--whole-archive -lgodcbegin -Wl,--no-whole-archive -lkos -lgodc -lparallax -lkosutils -ljpeg -lpng -lz -lkmg -lwav -ltremor

# Include project-specific overrides if they exist
-include godc.mk

all: $(OUTPUT)

$(TARGET).o: $(wildcard *.go)
	$(GCCGO) -c $^ -o $@

# Build romdisk if directory exists with files
ifneq ($(HAS_ROMDISK),)
romdisk.img: $(HAS_ROMDISK)
	$(KOS_BASE)/utils/genromfs/genromfs -f $@ -d $(ROMDISK_DIR)

romdisk.o: romdisk.img
	$(KOS_BASE)/utils/bin2o/bin2o $< romdisk $@
endif

$(OUTPUT): $(TARGET).o $(ROMDISK_OBJ)
	kos-cc -o $@ $^ $(LIB_PATHS) $(LIBS)

clean:
	rm -f *.o *.elf *.bin romdisk.img

.PHONY: all clean
