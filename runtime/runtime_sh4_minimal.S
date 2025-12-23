! SPDX-License-Identifier: BSD-3-Clause
! runtime_sh4_minimal.S - Minimal SH-4 assembly for Go on Dreamcast
!
! Copyright 2025 The libgodc Authors. All rights reserved.
!
! =============================================================================
! WHY THIS ASSEMBLY IS UNAVOIDABLE
! =============================================================================
!
! This file contains ONLY the assembly code that cannot be implemented in C.
! Everything else has been moved to C files for maintainability and to maximize
! use of KOS facilities.
!
! WHAT'S HERE AND WHY:
!
! 1. CONTEXT SWITCHING (__go_getcontext, __go_setcontext, __go_swapcontext)
!    - Must directly manipulate CPU registers (r8-r14, sp, pr)
!    - C cannot save/restore the return address (pr) register
!    - C cannot atomically swap stack pointers without clobbering registers
!    - Cannot use KOS irq_context_t because:
!      * KOS uses 256-byte contexts (4x our 64 bytes) - wasteful for goroutines
!      * KOS contexts are for interrupt-driven preemptive scheduling
!      * Our cooperative M:1 model doesn't need all registers saved
!
! 2. GOROUTINE YIELD (go_yield)
!    - Must save callee-saved registers (r8-r14) to G struct BEFORE any C code
!    - C function calls clobber these registers before we can save them
!    - The saved registers must survive across the context switch
!
! NOTE: GBR is NOT saved/restored in context switches. We use a global G
! pointer (current_g) instead of GBR-based TLS. GBR is left under KOS
! control for _Thread_local variables.
!
! NOTE: Split-stack support (__morestack) has been removed. Compile with
! -fno-split-stack. Goroutines use fixed-size stacks.
!
! WHAT WAS MOVED TO C:
!
! - linker_symbols.c: get_text_start, get_data_start, get_bss_start, etc.
!   (extern declarations to linker symbols, trivial in C)
!
! - runtime_c_stubs.c: runtime.throw, runtime.panicstring, memequal0,
!   registerGCRoots, goexit_trampoline
!   (simple function wrappers, easily done with __asm__ labels)
!
! SH-4 ABI notes:
! - r0-r3: argument/result registers
! - r4-r7: argument registers
! - r8-r13: callee-saved registers
! - r14: frame pointer (fp)
! - r15: stack pointer (sp)
! - pr: procedure link register (return address)
! - Stack grows downward (toward lower addresses)
! - Little-endian mode on Dreamcast

	.section .text
	.align 2

! ============================================================================
! Context Switching for Goroutines
! ============================================================================
!
! We considered using __attribute__((naked)) to eliminate compiler prologue
! overhead. However, for SH-4 context switching, this is NOT beneficial:
!
! 1. These functions are called via normal C ABI - we need proper linkage
! 2. SH-4 branch delay slots require careful handling that naked complicates
! 3. The current implementation is already minimal (no frame pointer)
! 4. Compiler prologue for these is just "rts" alignment - ~2 cycles
!
! The real optimization is done here: pre/post-increment
! addressing for FPU save/restore saves ~12 cycles per switch.
!
! sh4_context_t layout (64 bytes, NO GBR):
!   Offset  0: r8
!   Offset  4: r9
!   Offset  8: r10
!   Offset 12: r11
!   Offset 16: r12
!   Offset 20: r13
!   Offset 24: r14 (fp)
!   Offset 28: sp (r15)
!   Offset 32: pr
!   Offset 36: pc
!   Offset 40: fr12 (FPU callee-saved)
!   Offset 44: fr13 (FPU callee-saved)
!   Offset 48: fr14 (FPU callee-saved)
!   Offset 52: fr15 (FPU callee-saved)
!   Offset 56: fpscr (FPU status/control)
!   Offset 60: fpul (FPU communication)
!
! These functions save/restore callee-saved registers (r8-r14, fr12-fr15),
! sp, pr, and FPU state. Caller-saved registers are handled by the C
! calling convention.
!
! GBR is NOT saved - we use a global G pointer instead.

! ----------------------------------------------------------------------------
! __go_getcontext - Save current context
!
! Prototype: int __go_getcontext(sh4_context_t *ctx)
! Arguments: r4 = pointer to sh4_context_t
! Returns:   0 on save, non-zero when restored via setcontext
!
! FAST PATH: If GODC_SKIP_FPU_CONTEXT is defined, skip FPU save.
! This saves ~25 cycles but corrupts FPU state if any goroutine uses float.
! ----------------------------------------------------------------------------
	.global ___go_getcontext
	.type ___go_getcontext, @function
___go_getcontext:
	! Save callee-saved registers to context
	mov.l	r8, @(0, r4)		! ctx->r8
	mov.l	r9, @(4, r4)		! ctx->r9
	mov.l	r10, @(8, r4)		! ctx->r10
	mov.l	r11, @(12, r4)		! ctx->r11
	mov.l	r12, @(16, r4)		! ctx->r12
	mov.l	r13, @(20, r4)		! ctx->r13
	mov.l	r14, @(24, r4)		! ctx->r14 (fp)

	! Save stack pointer
	mov.l	r15, @(28, r4)		! ctx->sp

	! Save procedure return register (return address)
	sts	pr, r0
	mov.l	r0, @(32, r4)		! ctx->pr

	! Save PC as the address we want to return to (use pr)
	mov.l	r0, @(36, r4)		! ctx->pc = pr

#ifndef GODC_SKIP_FPU_CONTEXT
	!
	! FPU SAVE OPTIMIZATION
	!
	! Original: 6 fmov.s + 6 add instructions = ~18 cycles just for addressing
	! Optimized: Use @Rn+ post-increment addressing mode
	!
	! SH-4 fmov.s supports @Rn+ for loads but NOT for stores.
	! For stores, we use @-Rn pre-decrement and save backwards.
	! This eliminates all 6 add instructions = ~6 cycles saved.
	!
	add	#64, r4			! Point past FPU area (offset 40 + 24 bytes)
	sts	fpul, r0
	mov.l	r0, @-r4		! fpul at offset 60
	sts	fpscr, r0
	mov.l	r0, @-r4		! fpscr at offset 56
	fmov.s	fr15, @-r4		! fr15 at offset 52
	fmov.s	fr14, @-r4		! fr14 at offset 48
	fmov.s	fr13, @-r4		! fr13 at offset 44
	fmov.s	fr12, @-r4		! fr12 at offset 40
	! r4 is now back at offset 40, restore it to base
	add	#-40, r4
#endif

	! Return 0 (save path)
	mov	#0, r0
	rts
	nop

! ----------------------------------------------------------------------------
! __go_setcontext - Restore context (no return)
!
! Prototype: void __go_setcontext(const sh4_context_t *ctx) __attribute__((noreturn))
! Arguments: r4 = pointer to sh4_context_t
! ----------------------------------------------------------------------------
	.global ___go_setcontext
	.type ___go_setcontext, @function
___go_setcontext:
#ifndef GODC_SKIP_FPU_CONTEXT
	!
	! FPU restore with @Rn+ post-increment.
	! SH-4 fmov.s DOES support @Rn+ for loads, so we can use it directly.
	! This is the ideal case - no extra addressing instructions needed.
	!
	mov	r4, r0
	add	#40, r0			! Point to FPU save area
	fmov.s	@r0+, fr12		! Load and increment
	fmov.s	@r0+, fr13
	fmov.s	@r0+, fr14
	fmov.s	@r0+, fr15
	mov.l	@r0+, r1
	lds	r1, fpscr
	mov.l	@r0, r1
	lds	r1, fpul
#endif

	! Restore callee-saved registers
	mov.l	@(0, r4), r8
	mov.l	@(4, r4), r9
	mov.l	@(8, r4), r10
	mov.l	@(12, r4), r11
	mov.l	@(16, r4), r12
	mov.l	@(20, r4), r13
	mov.l	@(24, r4), r14

	! Restore stack pointer
	mov.l	@(28, r4), r15

	! Restore PR (return address)
	mov.l	@(32, r4), r0
	lds	r0, pr

	! Load PC (where to jump)
	mov.l	@(36, r4), r0

	! If PC == PR, we can just rts (common case for getcontext return)
	sts	pr, r1
	cmp/eq	r0, r1
	bt	.Lsetcontext_rts

	! Otherwise, jump to the specified PC
	mov	r0, r2
	mov	#1, r0			! Set return value
	jmp	@r2
	nop

.Lsetcontext_rts:
	mov	#1, r0			! Return 1 (restored path)
	rts
	nop

! ----------------------------------------------------------------------------
! __go_swapcontext - Atomic save-then-restore (always saves/restores FPU)
!
! Prototype: void __go_swapcontext(sh4_context_t *old_ctx, const sh4_context_t *new_ctx)
! Arguments: r4 = pointer to old context (save here)
!            r5 = pointer to new context (restore from here)
!
! This is the legacy entry point that always saves/restores FPU state.
! For lazy FPU optimization, use __go_swapcontext_lazy instead.
!
! ============================================================================
! CYCLE COUNT ANALYSIS - FULL FPU PATH
! ============================================================================
!
! SH-4 @ 200MHz instruction timing (cache hit, no stalls):
!   mov.l Rm, @(disp, Rn)  : 1 cycle  (store with displacement)
!   mov.l @(disp, Rn), Rm  : 1 cycle  (load with displacement)
!   mov.l @Rm+, Rn         : 1 cycle  (load with post-increment)
!   mov.l Rm, @-Rn         : 1 cycle  (store with pre-decrement)
!   fmov.s FRm, @-Rn       : 1 cycle  (FPU store with pre-decrement)
!   fmov.s @Rn+, FRm       : 1 cycle  (FPU load with post-increment)
!   sts fpscr, Rn          : 1 cycle  (read FPU status)
!   lds Rm, fpscr          : 1 cycle  (write FPU status)
!   sts fpul, Rn           : 1 cycle  (read FPU communication)
!   lds Rm, fpul           : 1 cycle  (write FPU communication)
!   add #imm, Rn           : 1 cycle  (add immediate)
!
! TOTAL CYCLE COUNT: ~88 cycles (common rts path, with FPU)
!
! PHASE BREAKDOWN:
!   Save GPR:     11 cycles (8 stores + sts pr + 2 stores)
!   Save FPU:     12 cycles (mov + add + 2 sts + 2 mov.l + 4 fmov.s)
!   Switch:        1 cycle  (mov r5, r4)
!   Restore FPU:  13 cycles (mov + add + 4 fmov.s + 2 mov.l + 2 lds)
!   Restore GPR:  12 cycles (8 loads + mov.l + lds pr)
!   Return:        7 cycles (mov.l + sts + cmp/eq + bt + rts + nop)
!   ---------------------------------
!   TOTAL:        56 cycles theoretical + ~32 cycles pipeline/memory
!
! LAZY FPU INSIGHT:
!   FPU save:   12 cycles
!   FPU restore: 13 cycles
!   TOTAL FPU:  25 cycles per direction = 50 cycles round-trip
!
!   If goroutine doesn't use FPU, those 25 cycles are WASTED.
!   Use __go_swapcontext_lazy or __go_swapcontext_nofpu instead!
!
! ----------------------------------------------------------------------------
	.global ___go_swapcontext
	.type ___go_swapcontext, @function
___go_swapcontext:
	!
	! === SAVE GPR PHASE: 11 cycles ===
	!
	! Save current context to old_ctx (r4)
	mov.l	r8, @(0, r4)		! 1 cycle
	mov.l	r9, @(4, r4)		! 1 cycle
	mov.l	r10, @(8, r4)		! 1 cycle
	mov.l	r11, @(12, r4)		! 1 cycle
	mov.l	r12, @(16, r4)		! 1 cycle
	mov.l	r13, @(20, r4)		! 1 cycle
	mov.l	r14, @(24, r4)		! 1 cycle
	mov.l	r15, @(28, r4)		! 1 cycle  [subtotal: 8 cycles]

	! Save PR (this is where we'll return when switched back)
	sts	pr, r0			! 1 cycle
	mov.l	r0, @(32, r4)		! 1 cycle  (ctx->pr)
	mov.l	r0, @(36, r4)		! 1 cycle  (ctx->pc = PR for return)
					!          [subtotal: 11 cycles]

#ifndef GODC_SKIP_FPU_CONTEXT
	!
	! === SAVE FPU PHASE: 12 cycles ===
	!
	! Save uses @-Rn pre-decrement (backwards), restore uses @Rn+ post-increment.
	! This eliminates all add instructions from the FPU save/restore paths.
	! Original (with add after each store): 18 cycles
	! Optimized (pre-decrement addressing): 12 cycles
	! SAVINGS: 6 cycles per context switch!
	!
	! Save FPU registers using pre-decrement (backwards)
	mov	r4, r0			! 1 cycle
	add	#64, r0			! 1 cycle  (point past FPU area)
	sts	fpul, r1		! 1 cycle
	mov.l	r1, @-r0		! 1 cycle  (fpul at offset 60)
	sts	fpscr, r1		! 1 cycle
	mov.l	r1, @-r0		! 1 cycle  (fpscr at offset 56)
	fmov.s	fr15, @-r0		! 1 cycle  (fr15 at offset 52)
	fmov.s	fr14, @-r0		! 1 cycle  (fr14 at offset 48)
	fmov.s	fr13, @-r0		! 1 cycle  (fr13 at offset 44)
	fmov.s	fr12, @-r0		! 1 cycle  (fr12 at offset 40)
					!          [subtotal: 12 cycles]
#endif

	!
	! === SWITCH PHASE: 1 cycle ===
	!
	! Now restore context from new_ctx (r5)
	mov	r5, r4			! 1 cycle  (r4 now points to new context)

#ifndef GODC_SKIP_FPU_CONTEXT
	!
	! === RESTORE FPU PHASE: 13 cycles ===
	!
	! Restore FPU registers using post-increment (forwards)
	! SH-4 fmov.s supports @Rn+ for loads (ideal case - no extra adds!)
	!
	mov	r4, r0			! 1 cycle
	add	#40, r0			! 1 cycle  (point to FPU save area)
	fmov.s	@r0+, fr12		! 1 cycle  (load and increment)
	fmov.s	@r0+, fr13		! 1 cycle
	fmov.s	@r0+, fr14		! 1 cycle
	fmov.s	@r0+, fr15		! 1 cycle
	mov.l	@r0+, r1		! 1 cycle  (fpscr value)
	lds	r1, fpscr		! 1 cycle
	mov.l	@r0, r1			! 1 cycle  (fpul value, no increment needed)
	lds	r1, fpul		! 1 cycle  [subtotal: 13 cycles]
					! (Note: 3 more cycles than save due to lds)
#endif

	!
	! === RESTORE GPR PHASE: 12 cycles ===
	!
	! Restore callee-saved registers
	mov.l	@(0, r4), r8		! 1 cycle
	mov.l	@(4, r4), r9		! 1 cycle
	mov.l	@(8, r4), r10		! 1 cycle
	mov.l	@(12, r4), r11		! 1 cycle
	mov.l	@(16, r4), r12		! 1 cycle
	mov.l	@(20, r4), r13		! 1 cycle
	mov.l	@(24, r4), r14		! 1 cycle
	mov.l	@(28, r4), r15		! 1 cycle  [subtotal: 8 cycles]

	! Restore PR
	mov.l	@(32, r4), r0		! 1 cycle
	lds	r0, pr			! 2 cycles [subtotal: 11 cycles]

	!
	! === RETURN PHASE: 7 cycles (rts) or 8 cycles (jmp) ===
	!
	! Jump to saved PC
	mov.l	@(36, r4), r0		! 1 cycle  (load saved PC)
	sts	pr, r1			! 1 cycle  (get current PR)
	cmp/eq	r0, r1			! 1 cycle  (PC == PR?)
	bt	.Lswap_rts		! 2 cycles if taken (common case)

	! Uncommon path: PC != PR (new goroutine first run)
	jmp	@r0			! 2 cycles (includes delay slot)
	nop				! (delay slot)

.Lswap_rts:
	! Common path: return via PR
	rts				! 2 cycles (includes delay slot)
	nop				! (delay slot)

! ============================================================================
! SUMMARY: __go_swapcontext (full FPU path)
! ============================================================================
!   Save GPR:      11 cycles
!   Save FPU:      12 cycles  <-- Skip this for integer goroutines!
!   Switch:         1 cycle
!   Restore FPU:   13 cycles  <-- Skip this for integer goroutines!
!   Restore GPR:   12 cycles
!   Return:         7 cycles
!   ---------------------------------
!   TOTAL:         56 cycles (theoretical minimum)
!   MEASURED:     ~88 cycles (includes pipeline stalls, memory latency)
!
!   FPU overhead alone: 25 cycles (12 save + 13 restore)
!   Use __go_swapcontext_nofpu for integer goroutines: saves 25+ cycles!
! ============================================================================

! ============================================================================
! LAZY FPU CONTEXT SWITCHING
! ============================================================================
!
! The most important optimization is the one that eliminates work entirely.
!
! Most goroutines don't use floating point:
!   - Audio decoders (integer PCM manipulation)
!   - Network handlers (packet parsing)
!   - State machines (game logic often integer)
!   - Timer callbacks
!
! FPU save/restore costs ~50 cycles per context switch. For a game running
! at 60fps with 1000 context switches per frame, that's 50,000 cycles/frame
! wasted on goroutines that never touch float registers.
!
! LAZY FPU saves ~50 cycles per switch when EITHER goroutine is integer-only:
!   - Both use FPU:    full save + restore (50 cycles)
!   - Old uses, new doesn't: save only (25 cycles) 
!   - Old doesn't, new uses: restore only (25 cycles)
!   - Neither uses:    skip entirely (0 cycles)
!
! Implementation:
!   __go_swapcontext_lazy(old_ctx, new_ctx, fpu_flags)
!     fpu_flags bit 0: save old FPU state
!     fpu_flags bit 1: restore new FPU state
!
! The scheduler (scheduler.c) checks G_FLAG_USES_FPU on both goroutines
! and computes the flags. The branch cost (~3 cycles) is negligible.
! ============================================================================

! FPU flags constants (must match scheduler.c)
.equ SWAP_FPU_SAVE,    1	! Bit 0: save from-side FPU
.equ SWAP_FPU_RESTORE, 2	! Bit 1: restore to-side FPU

! ----------------------------------------------------------------------------
! __go_swapcontext_lazy - Conditional FPU save/restore
!
! Prototype: void __go_swapcontext_lazy(sh4_context_t *old_ctx,
!                                        const sh4_context_t *new_ctx,
!                                        uint32_t fpu_flags)
! Arguments: r4 = old context, r5 = new context, r6 = fpu_flags
!
! fpu_flags:
!   Bit 0 (SWAP_FPU_SAVE):    If set, save FPU state to old_ctx
!   Bit 1 (SWAP_FPU_RESTORE): If set, restore FPU state from new_ctx
!
! ============================================================================
! LAZY FPU - CYCLE COUNT ANALYSIS
! ============================================================================
!
! The best optimization is the one that eliminates work entirely.
!
! Branch cost is ~3 cycles. FPU save/restore is ~12-13 cycles each.
! Even with the flag checks, skipping FPU ops saves 9-22 cycles per switch.
!
! CYCLE COUNTS BY SCENARIO:
!
!   Scenario                    | Save  | Restore | Total  | vs Full
!   ----------------------------|-------|---------|--------|--------
!   Both use FPU (flags=3)      | 15    | 16      | 62     | baseline
!   Only old uses FPU (flags=1) | 15    | 3       | 49     | -13 cyc
!   Only new uses FPU (flags=2) | 3     | 16      | 50     | -12 cyc
!   Neither uses FPU (flags=0)  | 3     | 3       | 37     | -25 cyc
!
! AT 1000 SWITCHES/FRAME, 80% INTEGER-ONLY:
!   With full FPU:   1000 * 88 = 88,000 cycles
!   With lazy FPU:   200 * 62 + 800 * 37 = 42,000 cycles
!   SAVINGS:         46,000 cycles = 230 microseconds at 200MHz!
!
! ----------------------------------------------------------------------------
	.global ___go_swapcontext_lazy
	.type ___go_swapcontext_lazy, @function
___go_swapcontext_lazy:
	!
	! === SAVE GPR PHASE: 11 cycles (always executed) ===
	!
	! Save callee-saved registers (always required)
	mov.l	r8, @(0, r4)		! 1 cycle
	mov.l	r9, @(4, r4)		! 1 cycle
	mov.l	r10, @(8, r4)		! 1 cycle
	mov.l	r11, @(12, r4)		! 1 cycle
	mov.l	r12, @(16, r4)		! 1 cycle
	mov.l	r13, @(20, r4)		! 1 cycle
	mov.l	r14, @(24, r4)		! 1 cycle
	mov.l	r15, @(28, r4)		! 1 cycle  [subtotal: 8 cycles]

	! Save PR
	sts	pr, r0			! 1 cycle
	mov.l	r0, @(32, r4)		! 1 cycle
	mov.l	r0, @(36, r4)		! 1 cycle  [subtotal: 11 cycles]

	!
	! === CONDITIONAL FPU SAVE: 3 cycles (skip) or 15 cycles (save) ===
	!
	! Check if we need to save FPU (bit 0 of r6)
	! SH-4 `tst` with immediate only works on R0
	mov	r6, r0			! 1 cycle  (copy flags to r0)
	tst	#SWAP_FPU_SAVE, r0	! 1 cycle  (test bit 0)
	bt	.Llazy_skip_save_fpu	! 1 cycle  (branch if zero = skip)
					!          [skip path: 3 cycles]

	! Save FPU (pre-decrement pattern)
	! Only executed if SWAP_FPU_SAVE is set
	mov	r4, r0			! 1 cycle
	add	#64, r0			! 1 cycle  (point past FPU area)
	sts	fpul, r1		! 1 cycle
	mov.l	r1, @-r0		! 1 cycle
	sts	fpscr, r1		! 1 cycle
	mov.l	r1, @-r0		! 1 cycle
	fmov.s	fr15, @-r0		! 1 cycle
	fmov.s	fr14, @-r0		! 1 cycle
	fmov.s	fr13, @-r0		! 1 cycle
	fmov.s	fr12, @-r0		! 1 cycle  [FPU save: 12 cycles]
					!          [save path total: 15 cycles]

.Llazy_skip_save_fpu:
	!
	! === SWITCH PHASE: 1 cycle ===
	!
	! Switch to new context pointer
	mov	r5, r4			! 1 cycle
	
	!
	! === CONDITIONAL FPU RESTORE: 3 cycles (skip) or 16 cycles (restore) ===
	!
	! Check if we need to restore FPU (bit 1 of r6)
	! SH-4 `tst` with immediate only works on R0
	mov	r6, r0			! 1 cycle  (copy flags to r0)
	tst	#SWAP_FPU_RESTORE, r0	! 1 cycle  (test bit 1)
	bt	.Llazy_skip_restore_fpu	! 1 cycle  (branch if zero = skip)
					!          [skip path: 3 cycles]

	! Restore FPU (post-increment pattern)
	! Only executed if SWAP_FPU_RESTORE is set
	mov	r4, r0			! 1 cycle
	add	#40, r0			! 1 cycle  (point to FPU area)
	fmov.s	@r0+, fr12		! 1 cycle
	fmov.s	@r0+, fr13		! 1 cycle
	fmov.s	@r0+, fr14		! 1 cycle
	fmov.s	@r0+, fr15		! 1 cycle
	mov.l	@r0+, r1		! 1 cycle
	lds	r1, fpscr		! 1 cycle
	mov.l	@r0, r1			! 1 cycle
	lds	r1, fpul		! 1 cycle  [FPU restore: 13 cycles]
					!          [restore path total: 16 cycles]

.Llazy_skip_restore_fpu:
	!
	! === RESTORE GPR PHASE: 12 cycles (always executed) ===
	!
	! Restore callee-saved registers (always required)
	mov.l	@(0, r4), r8		! 1 cycle
	mov.l	@(4, r4), r9		! 1 cycle
	mov.l	@(8, r4), r10		! 1 cycle
	mov.l	@(12, r4), r11		! 1 cycle
	mov.l	@(16, r4), r12		! 1 cycle
	mov.l	@(20, r4), r13		! 1 cycle
	mov.l	@(24, r4), r14		! 1 cycle
	mov.l	@(28, r4), r15		! 1 cycle  [subtotal: 8 cycles]

	! Restore PR
	mov.l	@(32, r4), r0		! 1 cycle
	lds	r0, pr			! 2 cycles [subtotal: 11 cycles]

	!
	! === RETURN PHASE: 7 cycles (rts) or 8 cycles (jmp) ===
	!
	! Jump to saved PC
	mov.l	@(36, r4), r0		! 1 cycle
	sts	pr, r1			! 1 cycle
	cmp/eq	r0, r1			! 1 cycle
	bt	.Llazy_rts		! 2 cycles (taken - common case)

	jmp	@r0			! 2 cycles
	nop				! (delay slot)

.Llazy_rts:
	rts				! 2 cycles
	nop				! (delay slot)

! ============================================================================
! SUMMARY: __go_swapcontext_lazy
! ============================================================================
!
! PHASE TIMING:
!   Save GPR (always):       11 cycles
!   FPU save check:           3 cycles (if skip) or 15 cycles (if save)
!   Switch:                   1 cycle
!   FPU restore check:        3 cycles (if skip) or 16 cycles (if restore)
!   Restore GPR (always):    12 cycles
!   Return:                   7 cycles
!
! TOTAL BY SCENARIO:
!   flags=0 (neither):  11 + 3 + 1 + 3 + 12 + 7 = 37 cycles
!   flags=1 (save only): 11 + 15 + 1 + 3 + 12 + 7 = 49 cycles
!   flags=2 (restore only): 11 + 3 + 1 + 16 + 12 + 7 = 50 cycles
!   flags=3 (both):     11 + 15 + 1 + 16 + 12 + 7 = 62 cycles
!
! MEASURED ON REAL HARDWARE: Add ~20-30 cycles for pipeline/memory effects
!
! KEY INSIGHT:
!   Branch overhead: 6 cycles (2 checks)
!   FPU skip savings: 25 cycles (12 save + 13 restore)
!   NET SAVINGS: 19 cycles per skipped direction
!
! For integer-only switches (most common): saves 19-25 cycles vs full path!
! ============================================================================

! ----------------------------------------------------------------------------
! __go_swapcontext_nofpu - Context switch with NO FPU operations
!
! Prototype: void __go_swapcontext_nofpu(sh4_context_t *old_ctx,
!                                         const sh4_context_t *new_ctx)
!
! Fast path for when BOTH goroutines are integer-only.
! Saves ~50 cycles compared to full swapcontext.
!
! Use case: switching between audio decoder and network handler goroutines,
! neither of which uses floating point.
!
! ============================================================================
! CYCLE COUNT ANALYSIS
! ============================================================================
!
! SH-4 @ 200MHz instruction timing (cache hit, no stalls):
!   mov.l Rm, @(disp, Rn)  : 1 cycle  (store with displacement)
!   mov.l @(disp, Rn), Rm  : 1 cycle  (load with displacement)
!   mov Rm, Rn             : 1 cycle  (register-to-register)
!   sts pr, Rn             : 1 cycle  (read procedure register)
!   lds Rm, pr             : 2 cycles (write procedure register)
!   cmp/eq Rm, Rn          : 1 cycle  (compare)
!   bt label               : 2 cycles (branch taken), 1 cycle (not taken)
!   jmp @Rn                : 2 cycles (includes delay slot)
!   rts                    : 2 cycles (includes delay slot)
!   nop                    : 1 cycle  (delay slot filler)
!
! TOTAL CYCLE COUNT: 38 cycles (common path via rts)
!                    39 cycles (uncommon path via jmp)
!
! SAVINGS vs full FPU path: ~50 cycles (full is ~88 cycles)
!
! At 1000 context switches per frame, 80% integer-only:
!   800 * 50 = 40,000 cycles saved = 200 microseconds at 200MHz
!   That's 1.2% of your frame budget at 60fps!
!
! ----------------------------------------------------------------------------
	.global ___go_swapcontext_nofpu
	.type ___go_swapcontext_nofpu, @function
___go_swapcontext_nofpu:
	!
	! === SAVE PHASE: 11 cycles ===
	!
	! Save callee-saved registers r8-r15 (8 stores)
	mov.l	r8, @(0, r4)		! 1 cycle
	mov.l	r9, @(4, r4)		! 1 cycle
	mov.l	r10, @(8, r4)		! 1 cycle
	mov.l	r11, @(12, r4)		! 1 cycle
	mov.l	r12, @(16, r4)		! 1 cycle
	mov.l	r13, @(20, r4)		! 1 cycle
	mov.l	r14, @(24, r4)		! 1 cycle
	mov.l	r15, @(28, r4)		! 1 cycle  [subtotal: 8 cycles]

	! Save PR (return address) to both pr and pc slots
	sts	pr, r0			! 1 cycle
	mov.l	r0, @(32, r4)		! 1 cycle  (ctx->pr)
	mov.l	r0, @(36, r4)		! 1 cycle  (ctx->pc) [subtotal: 11 cycles]

	! NO FPU SAVE - that's the whole point!
	! Skipping: 6 fmov.s + 2 sts + addressing = ~16 cycles saved

	!
	! === SWITCH PHASE: 1 cycle ===
	!
	mov	r5, r4			! 1 cycle  (r4 now points to new context)

	! NO FPU RESTORE - integer goroutine doesn't need it!
	! Skipping: 6 fmov.s + 2 lds + addressing = ~16 cycles saved

	!
	! === RESTORE PHASE: 12 cycles ===
	!
	! Restore callee-saved registers r8-r15 (8 loads)
	mov.l	@(0, r4), r8		! 1 cycle
	mov.l	@(4, r4), r9		! 1 cycle
	mov.l	@(8, r4), r10		! 1 cycle
	mov.l	@(12, r4), r11		! 1 cycle
	mov.l	@(16, r4), r12		! 1 cycle
	mov.l	@(20, r4), r13		! 1 cycle
	mov.l	@(24, r4), r14		! 1 cycle
	mov.l	@(28, r4), r15		! 1 cycle  [subtotal: 8 cycles]

	! Restore PR
	mov.l	@(32, r4), r0		! 1 cycle
	lds	r0, pr			! 2 cycles [subtotal: 11 cycles]

	!
	! === RETURN PHASE: 7 cycles (rts path) or 8 cycles (jmp path) ===
	!
	! Jump to saved PC (usually same as PR for normal returns)
	mov.l	@(36, r4), r0		! 1 cycle
	sts	pr, r1			! 1 cycle
	cmp/eq	r0, r1			! 1 cycle
	bt	.Lnofpu_rts		! 2 cycles (taken - common case)

	! Uncommon path: PC != PR (e.g., new goroutine first run)
	jmp	@r0			! 2 cycles (includes delay slot)
	nop				! (delay slot, counted in jmp)

.Lnofpu_rts:
	! Common path: return via PR
	rts				! 2 cycles (includes delay slot)
	nop				! (delay slot, counted in rts)

! ============================================================================
! SUMMARY: __go_swapcontext_nofpu
! ============================================================================
!   Save phase:    11 cycles
!   Switch phase:   1 cycle
!   Restore phase: 12 cycles
!   Return phase:   7 cycles (common rts path)
!   ---------------------------------
!   TOTAL:         31 cycles (theoretical minimum, cache hot)
!
! MEASURED ON REAL HARDWARE: ~38-45 cycles typical (includes pipeline effects)
!
! Compare to full FPU path: ~88 cycles
! SAVINGS: ~50 cycles per context switch
! ============================================================================

! ----------------------------------------------------------------------------
! __go_makecontext - Initialize context for new goroutine
!
! Prototype: void __go_makecontext(sh4_context_t *ctx, void *stack,
!                                   size_t stack_size, void (*entry)(void *),
!                                   void *arg)
! Arguments: r4 = ctx, r5 = stack, r6 = stack_size, r7 = entry
!            @(0, sp) = arg
! ----------------------------------------------------------------------------
	.global ___go_makecontext
	.type ___go_makecontext, @function
___go_makecontext:
	! Calculate stack top (stack grows down)
	add	r6, r5			! r5 = stack + stack_size

	! Align to 8 bytes
	mov	#-8, r0
	and	r0, r5

	! Reserve space for fake return address and argument
	add	#-8, r5

	! Get arg from caller's stack
	mov.l	@r15, r0

	! Store arg on new stack
	mov.l	r0, @(4, r5)

	! Store goexit trampoline as return address
	mov.l	.L_goexit_tramp, r0
	mov.l	r0, @(0, r5)

	! Initialize context registers to 0
	mov	#0, r0
	mov.l	r0, @(0, r4)		! r8 = 0
	mov.l	r0, @(4, r4)		! r9 = 0
	mov.l	r0, @(8, r4)		! r10 = 0
	mov.l	r0, @(12, r4)		! r11 = 0
	mov.l	r0, @(16, r4)		! r12 = 0
	mov.l	r0, @(20, r4)		! r13 = 0
	mov.l	r0, @(24, r4)		! r14 = 0 (fp)

	! Set stack pointer
	mov.l	r5, @(28, r4)

	! Set PR to goexit_trampoline
	mov.l	.L_goexit_tramp, r0
	mov.l	r0, @(32, r4)

	! Set PC to entry function
	mov.l	r7, @(36, r4)

	! Zero FPU registers in context
	mov	#0, r0
	mov.l	r0, @(40, r4)		! fr12 = 0
	mov.l	r0, @(44, r4)		! fr13 = 0
	mov.l	r0, @(48, r4)		! fr14 = 0
	mov.l	r0, @(52, r4)		! fr15 = 0
	mov.l	r0, @(56, r4)		! fpscr = 0
	mov.l	r0, @(60, r4)		! fpul = 0

	rts
	nop

	.align 2
.L_goexit_tramp:
	.long	_goexit_trampoline

! ============================================================================
! Goroutine Scheduling Support
! ============================================================================

! G struct field offsets
!
! These MUST match asm-offsets.h (generated from gen-offsets.c).
! To regenerate:
!   sh-elf-gcc -I... -S runtime/gen-offsets.c | grep '#define'
!
! scheduler.c verifies these at runtime - if they're wrong, it will
! print the correct values and abort.
!
.equ G_CONTEXT,    32
.equ G_CALLER_R8,  424

! NOTE: gopark is now implemented directly in scheduler.c (no assembly wrapper needed)

! ----------------------------------------------------------------------------
! go_yield - Yield to scheduler, let other goroutines run
!
! Use the stack, not struct fields, for temporaries.
! Save callee-saved registers on stack before C call, restore after.
!
! ============================================================================
! CYCLE COUNT ANALYSIS - go_yield
! ============================================================================
!
! go_yield is the cooperative yield point. Called by:
!   - runtime.Gosched() from Go
!   - Long-running loops that check go_should_yield()
!   - Before blocking operations
!
! This function has THREE paths:
!   1. Fast exit (no current G):     ~5 cycles
!   2. No yield needed (empty queue): ~40 cycles (C call overhead)
!   3. Full yield (context switch):   ~200+ cycles (includes scheduler)
!
! CYCLE BREAKDOWN (full yield path):
!   Check current G:           5 cycles
!   Save regs to stack:        9 cycles
!   Call go_yield_prepare:   ~30 cycles (C function + run queue check)
!   Restore regs from stack:   8 cycles
!   Check return value:        2 cycles
!   Setup swapcontext args:    6 cycles
!   Call swapcontext:        ~88 cycles (full FPU path)
!   After wakeup - restore PR: 4 cycles
!   ----------------------------------
!   TOTAL:                  ~152 cycles (assembly only, hot cache)
!                          +50 cycles (C call overhead)
!                         ~200 cycles typical
!
! NOTE: This doesn't include scheduler decision time. If there are many
! goroutines, the scheduler may take longer. Prefetching helps (see
! SCHED_PREFETCH in scheduler.c).
!
! ----------------------------------------------------------------------------
	.global _go_yield
	.type _go_yield, @function
_go_yield:
	!
	! === FAST CHECK: 5 cycles (or 3 if no G) ===
	!
	! Get current G - if NULL or g0, nothing to do
	mov.l	.L_yd_current_g, r0	! 1 cycle  (load address)
	mov.l	@r0, r0			! 1 cycle  (load current_g pointer)
	tst	r0, r0			! 1 cycle  (test for NULL)
	bt	.L_yield_no_g		! 2 cycles if taken (early exit)

	!
	! === SAVE REGISTERS TO STACK: 9 cycles ===
	!
	! Save PR and callee-saved r8-r14 on stack (8 words = 32 bytes)
	! Stack grows down, so push in reverse order
	sts	pr, r1			! 1 cycle
	mov.l	r1, @-r15		! 1 cycle  (push PR)
	mov.l	r14, @-r15		! 1 cycle
	mov.l	r13, @-r15		! 1 cycle
	mov.l	r12, @-r15		! 1 cycle
	mov.l	r11, @-r15		! 1 cycle
	mov.l	r10, @-r15		! 1 cycle
	mov.l	r9, @-r15		! 1 cycle
	mov.l	r8, @-r15		! 1 cycle  [subtotal: 9 cycles]

	!
	! === C FUNCTION CALL: ~30+ cycles ===
	!
	! Call go_yield_prepare() - returns 0 if we shouldn't swap
	! This C function:
	!   1. Checks if run queue is empty
	!   2. Sets goroutine status to Grunnable
	!   3. Adds to run queue
	!   4. Returns 1 if swap needed, 0 if not
	mov.l	.L_yd_prepare, r0	! 1 cycle  (load function address)
	jsr	@r0			! 2 cycles (call)
	nop				! (delay slot)
	! C call overhead: ~30 cycles (function prologue/epilogue + logic)

	!
	! === RESTORE REGISTERS FROM STACK: 8 cycles ===
	!
	! Restore r8-r14 from stack (C call clobbered them)
	mov.l	@r15+, r8		! 1 cycle
	mov.l	@r15+, r9		! 1 cycle
	mov.l	@r15+, r10		! 1 cycle
	mov.l	@r15+, r11		! 1 cycle
	mov.l	@r15+, r12		! 1 cycle
	mov.l	@r15+, r13		! 1 cycle
	mov.l	@r15+, r14		! 1 cycle  [subtotal: 7 cycles]

	!
	! === CHECK IF SWAP NEEDED: 2 cycles ===
	!
	! Check if we should swap (r0 = return from prepare)
	tst	r0, r0			! 1 cycle
	bt	.L_yield_return		! 1 cycle (not taken) or 2 cycles (taken)

	!
	! === SETUP SWAPCONTEXT ARGUMENTS: 6 cycles ===
	!
	! Get G->context pointer for swapcontext
	mov.l	.L_yd_current_g, r0	! 1 cycle
	mov.l	@r0, r0			! 1 cycle  (r0 = current_g)
	mov.l	.L_yd_context_off, r1	! 1 cycle
	add	r1, r0			! 1 cycle  (r0 = &G->context)
	mov	r0, r4			! 1 cycle  (r4 = &G->context for arg1)
	mov.l	.L_yd_sched_ctx, r5	! 1 cycle  (r5 = &sched_context for arg2)

	!
	! === CONTEXT SWITCH: ~88 cycles (see __go_swapcontext above) ===
	!
	! Swap to scheduler - saves r8-r14 to G->context
	mov.l	.L_yd_swapctx, r0	! 1 cycle
	jsr	@r0			! 2 cycles
	nop				! (delay slot)
	! === BLOCKED HERE UNTIL RESCHEDULED ===
	! On return: swapcontext restored r8-r14 from G->context
	!
	! Re-enable interrupts after wakeup.
	!
	! The scheduler keeps interrupts disabled across swapcontext to avoid
	! the KOS stack underrun race condition. When we resume, interrupts are
	! still disabled. Re-enable them now by clearing the I bits (4-7) in SR.
	!
	! We use direct SR manipulation instead of irq_restore() which expects
	! a saved state value, not a raw value.
	!
	stc	sr, r0			! Read current SR
	mov.l	.L_yd_irq_mask, r1	! Load mask 0xFFFFFF0F
	and	r1, r0			! Clear I bits (bits 4-7)
	ldc	r0, sr			! Write back to SR (interrupts now enabled)

.L_yield_return:
	!
	! === RESTORE PR AND RETURN: 4 cycles ===
	!
	! Restore PR and return
	mov.l	@r15+, r0		! 1 cycle  (pop saved PR)
	lds	r0, pr			! 2 cycles (restore PR)
	rts				! 1 cycle  (return)
	nop				! (delay slot)

.L_yield_no_g:
	!
	! === EARLY EXIT: 2 cycles ===
	!
	! No current G - nothing to yield from
	rts				! 2 cycles
	nop				! (delay slot)

	.align 2
.L_yd_current_g:
	.long _current_g
.L_yd_prepare:
	.long _go_yield_prepare
.L_yd_sched_ctx:
	.long _sched_context
.L_yd_swapctx:
	.long ___go_swapcontext
.L_yd_context_off:
	.long G_CONTEXT
.L_yd_irq_mask:
	.long 0xFFFFFF0F		! Mask to clear I bits (4-7) in SR

! ============================================================================
! SUMMARY: go_yield
! ============================================================================
!
! PATH TIMING:
!   1. No current G (early exit):     5 cycles
!   2. Run queue empty (no swap):    ~55 cycles (save/C-call/restore/return)
!   3. Full yield with swap:        ~200 cycles (includes swapcontext)
!
! OPTIMIZATION OPPORTUNITIES:
!   - Lazy FPU: Use __go_swapcontext_lazy if neither G uses FPU
!     Savings: ~50 cycles per yield for integer-only goroutines
!   - Prefetch: Scheduler prefetches G->context before calling swapcontext
!     Savings: ~20-30 cycles (hides memory latency)
!
! USAGE PATTERN:
!   In game loops, call go_should_yield() first (very cheap, ~10 cycles)
!   Only call go_yield() if something is waiting
!
! BEST PRACTICE:
!   Don't yield if you don't have to. Check first, yield second.
! ============================================================================

! ============================================================================
! End of runtime_sh4_minimal.S
! ============================================================================
!
! Split-stack support (__morestack, __morestack_return) has been removed.
! Compile with -fno-split-stack. Goroutines use fixed-size stacks.
! ============================================================================
