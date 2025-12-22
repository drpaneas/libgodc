# KOS Go bindings
GCCGO = sh-elf-gccgo
AR = sh-elf-ar
OBJCOPY = sh-elf-objcopy
KOS_BASE ?= /opt/toolchains/dc/kos

# Source files (gccgo builds only - exclude _stub.go files)
GO_SOURCES = types.go \
             timer.go \
             pvr.go \
             pvr_types.go \
             pvr_dr.go \
             input.go \
             sound.go \
             stream.go \
             video.go \
             math.go \
             bfont.go \
             plx.go \
             png.go \
             fs.go \
             vmu.go

C_SRCDIR = csrc
C_SOURCES = $(C_SRCDIR)/vram_stub.c $(C_SRCDIR)/plx_c_stub.c $(C_SRCDIR)/pvr_dr.c

# Compiler flags
GCCGO_FLAGS = -O2 -ml -m4-single -fno-split-stack -fgo-pkgpath=kos
KOS_CC_FLAGS = -O2 -ml -m4-single

all: kos.gox libkos.a

kos_go.o: $(GO_SOURCES)
	$(GCCGO) $(GCCGO_FLAGS) -c $(GO_SOURCES) -o $@

vram_stub.o: $(C_SRCDIR)/vram_stub.c
	kos-cc $(KOS_CC_FLAGS) -c $< -o $@

plx_c_stub.o: $(C_SRCDIR)/plx_c_stub.c
	kos-cc $(KOS_CC_FLAGS) -c $< -o $@

pvr_dr.o: $(C_SRCDIR)/pvr_dr.c
	kos-cc $(KOS_CC_FLAGS) -c $< -o $@

kos.o: kos_go.o vram_stub.o plx_c_stub.o pvr_dr.o
	sh-elf-ld -r -EL -o $@ kos_go.o vram_stub.o plx_c_stub.o pvr_dr.o

kos.gox: kos_go.o
	$(OBJCOPY) --dump-section .go_export=$@ $<

libkos.a: kos.o
	$(AR) rcs $@ $^

install: all
	cp kos.gox libkos.a $(KOS_BASE)/lib/

clean:
	rm -f *.o *.gox *.a

.PHONY: all install clean
