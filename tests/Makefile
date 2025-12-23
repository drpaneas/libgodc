# Test Suite for libgodc
#
# Usage:
#   make              - Build all tests
#   make test_print   - Build single test
#   make clean        - Remove all build artifacts
#
# NOTE: Tests link against the BUILD directory (../), not the installed
# KOS lib. This ensures tests always use the freshly built library.

# Dreamcast toolchain settings
GCCGO = sh-elf-gccgo
CC = kos-cc

# KOS paths
KOS_BASE ?= /opt/toolchains/dc/kos

# Link against BUILD directory, not installed KOS lib
LIBGODC_DIR = ..
LIBGODC_LIB = $(LIBGODC_DIR)
LIBGODC_LIBS = $(LIBGODC_DIR)/libgodc.a $(LIBGODC_DIR)/libgodcbegin.a

# Verbose mode: make V=1 to see actual commands
V ?= 0
ifeq ($(V),0)
  Q = @
  QUIET_GCCGO = @printf '  %-8s %s\n' GCCGO $@;
  QUIET_CC    = @printf '  %-8s %s\n' CC $@;
  QUIET_LD    = @printf '  %-8s %s\n' LD $@;
else
  Q =
  QUIET_GCCGO =
  QUIET_CC =
  QUIET_LD =
endif

# =============================================================================
# CORE TESTS (refactored December 2024)
# =============================================================================
#
# 5 core test files covering all runtime functionality:
#   test_print     - Basic I/O (println, print)
#   test_memory    - Allocation, slices, strings, GC
#   test_goroutines- Goroutines, channels, select, concurrency
#   test_types     - Maps, interfaces, structs, unsafe
#   test_control   - Defer, panic, recover
#
# 2 additional tests:
#   test_time      - Timer operations
#   test_timers    - Timer scheduling
#
# 2 benchmarks:
#   bench_architecture - Comprehensive architecture validation
#   bench_detailed     - Detailed performance analysis
#
# =============================================================================

# Core Go tests
GO_TESTS = \
	test_print \
	test_memory \
	test_goroutines \
	test_types \
	test_control \
	test_time \
	test_timers \
	test_alloc_inspect

# Benchmarks
GO_BENCHMARKS = \
	bench_architecture \
	bench_detailed \
	bench_gc_pause \
	bench_gc_techniques \
	bench_goroutine_usecase

# C tests (in c/ subdirectory)
C_TESTS = test_gc_internals test_gc_edge test_platform test_gc_percent test_free_external
C_TEST_DIR = c

# All test names
ALL_GO = $(GO_TESTS) $(GO_BENCHMARKS)
ALL_TESTS = $(ALL_GO) $(C_TESTS)

# Generated files
GO_ELFS = $(addsuffix .elf,$(ALL_GO))
GO_OBJS = $(addsuffix .o,$(ALL_GO))
C_TEST_ELFS = $(addsuffix .elf,$(C_TESTS))
C_TEST_OBJS = $(addprefix $(C_TEST_DIR)/,$(addsuffix .o,$(C_TESTS)))

# Include KOS rules for kos-cc
include $(KOS_BASE)/Makefile.rules

# Default target
all: $(GO_ELFS) $(C_TEST_ELFS)

# Go compiler flags - must match library build
GCCGO_FLAGS = -fno-omit-frame-pointer

# Compile Go source to object file
%.o: %.go
	$(QUIET_GCCGO)$(GCCGO) $(GCCGO_FLAGS) -c $< -o $@

# Link Go object file to ELF binary
%.elf: %.o $(LIBGODC_LIBS)
	$(QUIET_LD)$(CC) -o $@ $< \
		-Wl,--whole-archive $(LIBGODC_LIB)/libgodcbegin.a -Wl,--no-whole-archive \
		-L$(LIBGODC_LIB) -lgodc

# Compile C test source to object file
LIBGODC_INC = $(LIBGODC_DIR)/runtime
$(C_TEST_OBJS): $(C_TEST_DIR)/%.o: $(C_TEST_DIR)/%.c
	$(QUIET_CC)$(CC) -I$(LIBGODC_INC) -c $< -o $@

# Link C test object file to ELF binary
$(C_TEST_ELFS): %.elf: $(C_TEST_DIR)/%.o $(LIBGODC_LIBS)
	$(QUIET_LD)$(CC) -o $@ $< -L$(LIBGODC_LIB) -lgodc

# Individual test targets
$(ALL_TESTS): %: %.elf
	@echo "Built $@.elf"

# Clean build artifacts
clean:
	$(if $(filter 0,$(V)),@printf '  %-8s %s\n' CLEAN '*.o *.elf')
	$(Q)rm -f $(GO_OBJS) $(GO_ELFS) $(C_TEST_OBJS) $(C_TEST_ELFS)

# Remove only object files
tidy:
	$(if $(filter 0,$(V)),@printf '  %-8s %s\n' TIDY '*.o')
	$(Q)rm -f $(GO_OBJS) $(C_TEST_OBJS)

# Help
help:
	@echo "libgodc Test Suite (Refactored)"
	@echo ""
	@echo "Core Tests:"
	@echo "  test_print      - Basic I/O"
	@echo "  test_memory     - Allocation, slices, strings, GC"
	@echo "  test_goroutines - Goroutines, channels, select"
	@echo "  test_types      - Maps, interfaces, structs, unsafe"
	@echo "  test_control    - Defer, panic, recover"
	@echo "  test_time       - Timer operations"
	@echo "  test_timers     - Timer scheduling"
	@echo "  test_alloc_inspect - Memory allocation inspection (for docs)"
	@echo ""
	@echo "Benchmarks:"
	@echo "  bench_architecture - Comprehensive validation"
	@echo "  bench_detailed     - Detailed performance"
	@echo "  bench_gc_pause     - GC pause time measurements"
	@echo "  bench_gc_techniques - GC optimization techniques"
	@echo "  bench_goroutine_usecase - Goroutine use case comparison"
	@echo ""
	@echo "C Tests:"
	@echo "  test_gc_internals  - GC C-level tests"
	@echo "  test_gc_edge       - GC edge cases"
	@echo "  test_platform      - Platform tests"
	@echo ""
	@echo "Targets:"
	@echo "  make              - Build all"
	@echo "  make test_NAME    - Build single test"
	@echo "  make clean        - Remove all artifacts"
	@echo "  make tidy         - Remove only .o files"
	@echo "  make V=1          - Verbose mode"
	@echo ""

.PHONY: all clean tidy help $(ALL_TESTS)
