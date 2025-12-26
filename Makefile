# libgodc - Go runtime for Dreamcast
CC = sh-elf-gcc
AR = sh-elf-ar
AS = sh-elf-as
KOS_BASE ?= $(shell if [ -d "$(HOME)/dreamcast/kos" ]; then echo "$(HOME)/dreamcast/kos"; else echo "/opt/toolchains/dc/kos"; fi)

# -fno-split-stack: Disable split-stack, use fixed 64KB goroutine stacks.
#                   This removes the GBR conflict with KOS _Thread_local.
CFLAGS = -O2 -m4-single -ml -fno-builtin -Wall -Wextra -Werror -Wno-unused-parameter \
	-I$(KOS_BASE)/include -I$(KOS_BASE)/kernel/arch/dreamcast/include \
	-I$(KOS_BASE)/addons/include -Iruntime \
	-D_arch_dreamcast -DDREAMCAST -DLIBGODC_ACTUAL_BUILD -mfsrra -mfsca \
	-ffunction-sections -fdata-sections -matomic-model=soft-imask \
	-fno-split-stack -fno-omit-frame-pointer

ifdef DEBUG
CFLAGS += -DLIBGODC_DEBUG=$(DEBUG) -g
endif

SRCS = $(filter-out runtime/gen-offsets.c, $(wildcard runtime/*.c))
# Use minimal assembly - most stubs moved to C (runtime_c_stubs.c)
OBJS = $(SRCS:.c=.o) runtime/runtime_sh4_minimal.o

all: libgodc.a libgodcbegin.a

# Generate struct offsets for assembly. Regenerate after changing G struct.
# The offsets are verified at runtime by scheduler.c.
runtime/asm-offsets.h: runtime/gen-offsets.c runtime/goroutine.h
	@echo "Generating asm-offsets.h..."
	@$(CC) $(CFLAGS) -S -o - $< 2>/dev/null | grep '#define' | sed 's/#define //' | \
		awk 'BEGIN { \
			print "/* AUTO-GENERATED from gen-offsets.c - DO NOT EDIT */"; \
			print "#ifndef ASM_OFFSETS_H"; \
			print "#define ASM_OFFSETS_H"; \
		} { gsub(/#/, ""); print "#define " $$0 } \
		END { print "#endif" }' > $@

# Ensure asm-offsets.h exists before compiling scheduler.c
runtime/scheduler.o: runtime/scheduler.c runtime/asm-offsets.h
	$(MAKE) -C kos
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Minimal assembly: only context switching
runtime/runtime_sh4_minimal.o: runtime/runtime_sh4_minimal.S
	$(AS) -little --isa=sh4a $< -o $@

libgodc.a: $(OBJS)
	$(AR) rcs $@ $^

libgodcbegin.a: runtime/go-main.o runtime/romdisk_init.o runtime/kos_startup.o
	$(AR) rcs $@ $^

install: all
	mkdir -p $(KOS_BASE)/lib $(KOS_BASE)/include/godc
	cp libgodc.a libgodcbegin.a $(KOS_BASE)/lib/
	cp runtime/*.h $(KOS_BASE)/include/godc/
	$(MAKE) -C kos install

clean:
	rm -f runtime/*.o *.a
	rm -f examples/*/*.elf examples/*/*.o
	rm -f tests/*.elf tests/*.o tests/c/*.o
	$(MAKE) -C kos clean

# Verify asm-offsets.h is up-to-date with current G struct layout.
# This regenerates asm-offsets.h and checks if it matches the committed version.
# Run after modifying the G struct in goroutine.h or gen-offsets.c.
#
# The workflow for updating G struct:
# 1. Modify runtime/goroutine.h (the authoritative definition)
# 2. Update runtime/gen-offsets.c to match (copy struct layout)
# 3. Run 'make check-offsets' - it will fail if out of sync
# 4. Run 'make runtime/asm-offsets.h' to regenerate
# 5. Update runtime/runtime_sh4_minimal.S with new offsets (if G_CONTEXT changed)
# 6. Run 'make check-offsets' again - should pass now
# 7. The runtime also verifies at startup (scheduler.c::verify_asm_offsets)
check-offsets: runtime/gen-offsets.c runtime/goroutine.h
	@echo "Checking asm-offsets.h synchronization..."
	@$(CC) $(CFLAGS) -S -o /tmp/gen-offsets-check.s runtime/gen-offsets.c 2>/dev/null && \
		grep '#define' /tmp/gen-offsets-check.s | sed 's/#define //' | \
		awk 'BEGIN { print "/* AUTO-GENERATED from gen-offsets.c - DO NOT EDIT */" \
			; print "#ifndef ASM_OFFSETS_H"; print "#define ASM_OFFSETS_H" } \
		{ gsub(/#/, ""); print "#define " $$0 } \
		END { print "#endif" }' > /tmp/asm-offsets-check.h && \
	diff -q /tmp/asm-offsets-check.h runtime/asm-offsets.h > /dev/null 2>&1 || \
		(echo "ERROR: asm-offsets.h is out of sync with G struct!"; \
		 echo "Expected:"; cat /tmp/asm-offsets-check.h; \
		 echo ""; \
		 echo "Actual runtime/asm-offsets.h:"; cat runtime/asm-offsets.h; \
		 echo ""; \
		 echo "Run 'make runtime/asm-offsets.h' to regenerate."; \
		 rm -f /tmp/gen-offsets-check.s /tmp/asm-offsets-check.h; \
		 exit 1)
	@rm -f /tmp/gen-offsets-check.s /tmp/asm-offsets-check.h
	@echo "asm-offsets.h is in sync with G struct."

.PHONY: all install clean check-offsets
