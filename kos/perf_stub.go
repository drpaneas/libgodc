//go:build !gccgo

package kos

// Performance counter event modes (from KOS perf_cntr_event_t)
const (
	PMCR_INIT_NO_MODE                        uint32 = 0x00 // Disable counter
	PMCR_OPERAND_READ_ACCESS_MODE            uint32 = 0x01 // Operand read access
	PMCR_OPERAND_WRITE_ACCESS_MODE           uint32 = 0x02 // Operand write access
	PMCR_UTLB_MISS_MODE                      uint32 = 0x03 // UTLB miss
	PMCR_OPERAND_CACHE_READ_MISS_MODE        uint32 = 0x04 // Operand cache read miss
	PMCR_OPERAND_CACHE_WRITE_MISS_MODE       uint32 = 0x05 // Operand cache write miss
	PMCR_INSTRUCTION_FETCH_MODE              uint32 = 0x06 // Instruction fetch
	PMCR_INSTRUCTION_TLB_MISS_MODE           uint32 = 0x07 // Instruction TLB miss
	PMCR_INSTRUCTION_CACHE_MISS_MODE         uint32 = 0x08 // Instruction cache miss
	PMCR_ALL_OPERAND_ACCESS_MODE             uint32 = 0x09 // All operand accesses
	PMCR_ALL_INSTRUCTION_FETCH_MODE          uint32 = 0x0a // All instruction fetches
	PMCR_ON_CHIP_RAM_OPERAND_ACCESS_MODE     uint32 = 0x0b // On-chip RAM operand access
	PMCR_ON_CHIP_IO_ACCESS_MODE              uint32 = 0x0d // On-chip I/O access
	PMCR_OPERAND_ACCESS_MODE                 uint32 = 0x0e // Operand access
	PMCR_OPERAND_CACHE_MISS_MODE             uint32 = 0x0f // Operand cache miss
	PMCR_BRANCH_ISSUED_MODE                  uint32 = 0x10 // Branch issued
	PMCR_BRANCH_TAKEN_MODE                   uint32 = 0x11 // Branch taken
	PMCR_SUBROUTINE_ISSUED_MODE              uint32 = 0x12 // Subroutine issued
	PMCR_INSTRUCTION_ISSUED_MODE             uint32 = 0x13 // Instruction issued
	PMCR_PARALLEL_INSTRUCTION_ISSUED_MODE    uint32 = 0x14 // Parallel instruction issued
	PMCR_FPU_INSTRUCTION_ISSUED_MODE         uint32 = 0x15 // FPU instruction issued
	PMCR_INTERRUPT_COUNTER_MODE              uint32 = 0x16 // Interrupt counter
	PMCR_NMI_COUNTER_MODE                    uint32 = 0x17 // NMI counter
	PMCR_TRAPA_INSTRUCTION_COUNTER_MODE      uint32 = 0x18 // TRAPA instruction counter
	PMCR_UBC_A_MATCH_MODE                    uint32 = 0x19 // UBC A match
	PMCR_UBC_B_MATCH_MODE                    uint32 = 0x1a // UBC B match
	PMCR_INSTRUCTION_CACHE_FILL_MODE         uint32 = 0x21 // Instruction cache fill
	PMCR_OPERAND_CACHE_FILL_MODE             uint32 = 0x22 // Operand cache fill
	PMCR_ELAPSED_TIME_MODE                   uint32 = 0x23 // Elapsed time (CPU cycles)
	PMCR_PIPELINE_FREEZE_BY_ICACHE_MISS_MODE uint32 = 0x24 // Pipeline freeze by I-cache miss
	PMCR_PIPELINE_FREEZE_BY_DCACHE_MISS_MODE uint32 = 0x25 // Pipeline freeze by D-cache miss
	PMCR_PIPELINE_FREEZE_BY_BRANCH_MODE      uint32 = 0x27 // Pipeline freeze by branch
	PMCR_PIPELINE_FREEZE_BY_CPU_REGISTER_MODE uint32 = 0x28 // Pipeline freeze by CPU register
	PMCR_PIPELINE_FREEZE_BY_FPU_MODE         uint32 = 0x29 // Pipeline freeze by FPU
)

// Legacy aliases for backwards compatibility
const (
	PERF_COUNTER_DISABLE   = PMCR_INIT_NO_MODE
	PERF_COUNTER_CYCLES    = PMCR_ELAPSED_TIME_MODE // Use elapsed time for cycle counting
	PERF_COUNTER_OC_MISS   = PMCR_OPERAND_CACHE_MISS_MODE
	PERF_COUNTER_IC_MISS   = PMCR_INSTRUCTION_CACHE_MISS_MODE
	PERF_COUNTER_BRANCH    = PMCR_BRANCH_ISSUED_MODE
	PERF_COUNTER_UTLB_MISS = PMCR_UTLB_MISS_MODE
)

// Clock type constants (from KOS perf_cntr_clock_t)
const (
	PMCR_COUNT_CPU_CYCLES   int32 = 0 // Count CPU clock cycles
	PMCR_COUNT_RATIO_CYCLES int32 = 1 // Count by CPU/bus clock ratio
)

func perfGetCycles(counter int32) uint64                        { return 0 }
func perfCntrStart(counter int32, mode uint32, clockType int32) {}
func perfCntrStop(counter int32)                                {}
func perfCntrClear(counter int32)                               {}
func PerfCntrStart()                                            {}
func PerfCntrStartMode(counter int32, mode uint32)              {}
func PerfCntrStartFull(counter int32, mode uint32, clockType int32) {}
func PerfCntrStop()                                             {}
func PerfCntrCycles() uint64                                    { return 0 }
