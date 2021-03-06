
/*  cpu_asm.s	1.1 - 95/12/04
 *
 *  This file contains the assembly code for the PowerPC implementation
 *  of RTEMS.
 *
 *  Author:	Andrew Bray <andy@i-cubed.co.uk>
 *
 *  COPYRIGHT (c) 1995 by i-cubed ltd.
 *
 *  To anyone who acknowledges that this file is provided "AS IS"
 *  without any express or implied warranty:
 *      permission to use, copy, modify, and distribute this file
 *      for any purpose is hereby granted without fee, provided that
 *      the above copyright notice and this notice appears in all
 *      copies, and that the name of i-cubed limited not be used in
 *      advertising or publicity pertaining to distribution of the
 *      software without specific, written prior permission.
 *      i-cubed limited makes no representations about the suitability
 *      of this software for any purpose.
 *
 *  Derived from c/src/exec/cpu/no_cpu/cpu_asm.c:
 *
 *  COPYRIGHT (c) 1989-1997.
 *  On-Line Applications Research Corporation (OAR).
 *
 *  Copyright (c) 2011 embedded brains GmbH.
 *
 *  The license and distribution terms for this file may in
 *  the file LICENSE in this distribution or at
 *  http://www.rtems.com/license/LICENSE.
 *
 *  $Id$
 */

#include <rtems/asm.h>
#include <rtems/powerpc/powerpc.h>
#include <rtems/score/cpu.h>
#include <bspopts.h>

#if BSP_DATA_CACHE_ENABLED && PPC_CACHE_ALIGNMENT == 32
  #define DATA_CACHE_ALIGNMENT(reg) \
    li reg, PPC_CACHE_ALIGNMENT
  #define DATA_CACHE_ZERO(rega, regb) \
    dcbz rega, regb
  #define DATA_CACHE_TOUCH(rega, regb) \
    dcbt rega, regb
  #define DATA_CACHE_ZERO_AND_TOUCH(reg, offset) \
    li reg, offset; dcbz reg, r3; dcbt reg, r4
#else
  #define DATA_CACHE_ALIGNMENT(reg)
  #define DATA_CACHE_ZERO(rega, regb)
  #define DATA_CACHE_TOUCH(rega, regb)
  #define DATA_CACHE_ZERO_AND_TOUCH(reg, offset) \
    li reg, offset
#endif

/*
 * Offsets for various Contexts
 */
	.set	GP_1, 0
	.set	GP_2, (GP_1 + 4)
	.set	GP_13, (GP_2 + 4)
	.set	GP_14, (GP_13 + 4)

	.set	GP_15, (GP_14 + 4)
	.set	GP_16, (GP_15 + 4)
	.set	GP_17, (GP_16 + 4)
	.set	GP_18, (GP_17 + 4)

	.set	GP_19, (GP_18 + 4)
	.set	GP_20, (GP_19 + 4)
	.set	GP_21, (GP_20 + 4)
	.set	GP_22, (GP_21 + 4)

	.set	GP_23, (GP_22 + 4)
	.set	GP_24, (GP_23 + 4)
	.set	GP_25, (GP_24 + 4)
	.set	GP_26, (GP_25 + 4)

	.set	GP_27, (GP_26 + 4)
	.set	GP_28, (GP_27 + 4)
	.set	GP_29, (GP_28 + 4)
	.set	GP_30, (GP_29 + 4)

	.set	GP_31, (GP_30 + 4)
	.set	GP_CR, (GP_31 + 4)
	.set	GP_PC, (GP_CR + 4)
	.set	GP_MSR, (GP_PC + 4)

#if (PPC_HAS_DOUBLE==1)
	.set	FP_SIZE,	8
#define	LDF	lfd
#define	STF	stfd
#else
	.set	FP_SIZE,	4
#define	LDF	lfs
#define	STF	stfs
#endif

	.set	FP_0, 0
	.set	FP_1, (FP_0 + FP_SIZE)
	.set	FP_2, (FP_1 + FP_SIZE)
	.set	FP_3, (FP_2 + FP_SIZE)
	.set	FP_4, (FP_3 + FP_SIZE)
	.set	FP_5, (FP_4 + FP_SIZE)
	.set	FP_6, (FP_5 + FP_SIZE)
	.set	FP_7, (FP_6 + FP_SIZE)
	.set	FP_8, (FP_7 + FP_SIZE)
	.set	FP_9, (FP_8 + FP_SIZE)
	.set	FP_10, (FP_9 + FP_SIZE)
	.set	FP_11, (FP_10 + FP_SIZE)
	.set	FP_12, (FP_11 + FP_SIZE)
	.set	FP_13, (FP_12 + FP_SIZE)
	.set	FP_14, (FP_13 + FP_SIZE)
	.set	FP_15, (FP_14 + FP_SIZE)
	.set	FP_16, (FP_15 + FP_SIZE)
	.set	FP_17, (FP_16 + FP_SIZE)
	.set	FP_18, (FP_17 + FP_SIZE)
	.set	FP_19, (FP_18 + FP_SIZE)
	.set	FP_20, (FP_19 + FP_SIZE)
	.set	FP_21, (FP_20 + FP_SIZE)
	.set	FP_22, (FP_21 + FP_SIZE)
	.set	FP_23, (FP_22 + FP_SIZE)
	.set	FP_24, (FP_23 + FP_SIZE)
	.set	FP_25, (FP_24 + FP_SIZE)
	.set	FP_26, (FP_25 + FP_SIZE)
	.set	FP_27, (FP_26 + FP_SIZE)
	.set	FP_28, (FP_27 + FP_SIZE)
	.set	FP_29, (FP_28 + FP_SIZE)
	.set	FP_30, (FP_29 + FP_SIZE)
	.set	FP_31, (FP_30 + FP_SIZE)
	.set	FP_FPSCR, (FP_31 + FP_SIZE)

	.set	IP_LINK, 0
	.set	IP_0, (IP_LINK + 8)
	.set	IP_2, (IP_0 + 4)

	.set	IP_3, (IP_2 + 4)
	.set	IP_4, (IP_3 + 4)
	.set	IP_5, (IP_4 + 4)
	.set	IP_6, (IP_5 + 4)

	.set	IP_7, (IP_6 + 4)
	.set	IP_8, (IP_7 + 4)
	.set	IP_9, (IP_8 + 4)
	.set	IP_10, (IP_9 + 4)

	.set	IP_11, (IP_10 + 4)
	.set	IP_12, (IP_11 + 4)
	.set	IP_13, (IP_12 + 4)
	.set	IP_28, (IP_13 + 4)

	.set	IP_29, (IP_28 + 4)
	.set	IP_30, (IP_29 + 4)
	.set	IP_31, (IP_30 + 4)
	.set	IP_CR, (IP_31 + 4)

	.set	IP_CTR, (IP_CR + 4)
	.set	IP_XER, (IP_CTR + 4)
	.set	IP_LR, (IP_XER + 4)
	.set	IP_PC, (IP_LR + 4)

	.set	IP_MSR, (IP_PC + 4)
	.set	IP_END, (IP_MSR + 16)

	BEGIN_CODE
/*
 *  _CPU_Context_save_fp_context
 *
 *  This routine is responsible for saving the FP context
 *  at *fp_context_ptr.  If the point to load the FP context
 *  from is changed then the pointer is modified by this routine.
 *
 *  Sometimes a macro implementation of this is in cpu.h which dereferences
 *  the ** and a similarly named routine in this file is passed something
 *  like a (Context_Control_fp *).  The general rule on making this decision
 *  is to avoid writing assembly language.
 */

	ALIGN (PPC_CACHE_ALIGNMENT, PPC_CACHE_ALIGN_POWER)
	PUBLIC_PROC (_CPU_Context_save_fp)
PROC (_CPU_Context_save_fp):
#if (PPC_HAS_FPU == 1)
/* A FP context switch may occur in an ISR or exception handler when the FPU is not
 * available. Therefore, we must explicitely enable it here!
 */
	mfmsr	r4
	andi.	r5,r4,MSR_FP
	bne	1f
	ori	r5,r4,MSR_FP
	mtmsr	r5
	isync
1:
	lwz	r3, 0(r3)
	STF	f0, FP_0(r3)
	STF	f1, FP_1(r3)
	STF	f2, FP_2(r3)
	STF	f3, FP_3(r3)
	STF	f4, FP_4(r3)
	STF	f5, FP_5(r3)
	STF	f6, FP_6(r3)
	STF	f7, FP_7(r3)
	STF	f8, FP_8(r3)
	STF	f9, FP_9(r3)
	STF	f10, FP_10(r3)
	STF	f11, FP_11(r3)
	STF	f12, FP_12(r3)
	STF	f13, FP_13(r3)
	STF	f14, FP_14(r3)
	STF	f15, FP_15(r3)
	STF	f16, FP_16(r3)
	STF	f17, FP_17(r3)
	STF	f18, FP_18(r3)
	STF	f19, FP_19(r3)
	STF	f20, FP_20(r3)
	STF	f21, FP_21(r3)
	STF	f22, FP_22(r3)
	STF	f23, FP_23(r3)
	STF	f24, FP_24(r3)
	STF	f25, FP_25(r3)
	STF	f26, FP_26(r3)
	STF	f27, FP_27(r3)
	STF	f28, FP_28(r3)
	STF	f29, FP_29(r3)
	STF	f30, FP_30(r3)
	STF	f31, FP_31(r3)
	mffs	f2
	STF	f2, FP_FPSCR(r3)
	bne	1f
	mtmsr	r4
	isync
1:
#endif
	blr

/*
 *  _CPU_Context_restore_fp_context
 *
 *  This routine is responsible for restoring the FP context
 *  at *fp_context_ptr.  If the point to load the FP context
 *  from is changed then the pointer is modified by this routine.
 *
 *  Sometimes a macro implementation of this is in cpu.h which dereferences
 *  the ** and a similarly named routine in this file is passed something
 *  like a (Context_Control_fp *).  The general rule on making this decision
 *  is to avoid writing assembly language.
 */

	ALIGN (PPC_CACHE_ALIGNMENT, PPC_CACHE_ALIGN_POWER)
	PUBLIC_PROC (_CPU_Context_restore_fp)
PROC (_CPU_Context_restore_fp):
#if (PPC_HAS_FPU == 1)
	lwz	r3, 0(r3)
/* A FP context switch may occur in an ISR or exception handler when the FPU is not
 * available. Therefore, we must explicitely enable it here!
 */
	mfmsr	r4
	andi.	r5,r4,MSR_FP
	bne	1f
	ori	r5,r4,MSR_FP
	mtmsr	r5
	isync
1:
	LDF	f2, FP_FPSCR(r3)
	mtfsf	255, f2
	LDF	f0, FP_0(r3)
	LDF	f1, FP_1(r3)
	LDF	f2, FP_2(r3)
	LDF	f3, FP_3(r3)
	LDF	f4, FP_4(r3)
	LDF	f5, FP_5(r3)
	LDF	f6, FP_6(r3)
	LDF	f7, FP_7(r3)
	LDF	f8, FP_8(r3)
	LDF	f9, FP_9(r3)
	LDF	f10, FP_10(r3)
	LDF	f11, FP_11(r3)
	LDF	f12, FP_12(r3)
	LDF	f13, FP_13(r3)
	LDF	f14, FP_14(r3)
	LDF	f15, FP_15(r3)
	LDF	f16, FP_16(r3)
	LDF	f17, FP_17(r3)
	LDF	f18, FP_18(r3)
	LDF	f19, FP_19(r3)
	LDF	f20, FP_20(r3)
	LDF	f21, FP_21(r3)
	LDF	f22, FP_22(r3)
	LDF	f23, FP_23(r3)
	LDF	f24, FP_24(r3)
	LDF	f25, FP_25(r3)
	LDF	f26, FP_26(r3)
	LDF	f27, FP_27(r3)
	LDF	f28, FP_28(r3)
	LDF	f29, FP_29(r3)
	LDF	f30, FP_30(r3)
	LDF	f31, FP_31(r3)
	bne	1f
	mtmsr	r4
	isync
1:
#endif
	blr

/*  _CPU_Context_switch
 *
 *  This routine performs a normal non-FP context switch.
 */
	ALIGN (PPC_CACHE_ALIGNMENT, PPC_CACHE_ALIGN_POWER)
	PUBLIC_PROC (_CPU_Context_switch)
PROC (_CPU_Context_switch):
#ifndef __SPE__
	sync
	isync
	/* This assumes that all the registers are in the given order */
	DATA_CACHE_ALIGNMENT(r5)
	addi	r9,r3,-4
	DATA_CACHE_ZERO(r5, r9)
#ifdef RTEMS_MULTIPROCESSING
	/*
	 * We have to clear the reservation of the executing thread.  See also
	 * Book E section 6.1.6.2 "Atomic Update Primitives".
	 */
	li	r10, GP_1 + 4
	stwcx.	r1, r9, r10
#endif
	stw	r1, GP_1+4(r9)
	stw	r2, GP_2+4(r9)
#if (PPC_USE_MULTIPLE == 1)
	addi	r9, r9, GP_18+4
	DATA_CACHE_ZERO(r5, r9)
	stmw	r13, GP_13-GP_18(r9)
#else
	stw	r13, GP_13+4(r9)
	stw	r14, GP_14+4(r9)
	stw	r15, GP_15+4(r9)
	stw	r16, GP_16+4(r9)
	stw	r17, GP_17+4(r9)
	stwu	r18, GP_18+4(r9)
	DATA_CACHE_ZERO(r5, r9)
	stw	r19, GP_19-GP_18(r9)
	stw	r20, GP_20-GP_18(r9)
	stw	r21, GP_21-GP_18(r9)
	stw	r22, GP_22-GP_18(r9)
	stw	r23, GP_23-GP_18(r9)
	stw	r24, GP_24-GP_18(r9)
	stw	r25, GP_25-GP_18(r9)
	stw	r26, GP_26-GP_18(r9)
	stw	r27, GP_27-GP_18(r9)
	stw	r28, GP_28-GP_18(r9)
	stw	r29, GP_29-GP_18(r9)
	stw	r30, GP_30-GP_18(r9)
	stw	r31, GP_31-GP_18(r9)
#endif
	DATA_CACHE_TOUCH(r0, r4)
	mfcr	r6
	stw	r6, GP_CR-GP_18(r9)
	mflr	r7
	stw	r7, GP_PC-GP_18(r9)
	mfmsr	r8
	stw	r8, GP_MSR-GP_18(r9)

#ifdef __ALTIVEC__
	mr      r14, r4 
	EXTERN_PROC(_CPU_Context_switch_altivec)
	bl		_CPU_Context_switch_altivec
	mr      r4, r14
	DATA_CACHE_ALIGNMENT(r5)
#endif

	DATA_CACHE_TOUCH(r5, r4)
	lwz	r1, GP_1(r4)
	lwz	r2, GP_2(r4)
#if (PPC_USE_MULTIPLE == 1)
	addi	r4, r4, GP_19
	DATA_CACHE_TOUCH(r5, r4)
	lmw	r13, GP_13-GP_19(r4)
#else
	lwz	r13, GP_13(r4)
	lwz	r14, GP_14(r4)
	lwz	r15, GP_15(r4)
	lwz	r16, GP_16(r4)
	lwz	r17, GP_17(r4)
	lwz	r18, GP_18(r4)
	lwzu	r19, GP_19(r4)
	DATA_CACHE_TOUCH(r5, r4)
	lwz	r20, GP_20-GP_19(r4)
	lwz	r21, GP_21-GP_19(r4)
	lwz	r22, GP_22-GP_19(r4)
	lwz	r23, GP_23-GP_19(r4)
	lwz	r24, GP_24-GP_19(r4)
	lwz	r25, GP_25-GP_19(r4)
	lwz	r26, GP_26-GP_19(r4)
	lwz	r27, GP_27-GP_19(r4)
	lwz	r28, GP_28-GP_19(r4)
	lwz	r29, GP_29-GP_19(r4)
	lwz	r30, GP_30-GP_19(r4)
	lwz	r31, GP_31-GP_19(r4)
#endif
	lwz	r6, GP_CR-GP_19(r4)
	lwz	r7, GP_PC-GP_19(r4)
	lwz	r8, GP_MSR-GP_19(r4)
	mtcrf	255, r6
	mtlr	r7
	mtmsr	r8
	isync

	blr
#else /* __SPE__ */
	/* Align to a cache line */
	clrrwi	r3, r3, 5
	clrrwi	r4, r4, 5

	DATA_CACHE_ZERO_AND_TOUCH(r10, PPC_CONTEXT_CACHE_LINE_0)
	DATA_CACHE_ZERO_AND_TOUCH(r11, PPC_CONTEXT_CACHE_LINE_1)

	/* Save context to r3 */

	mfmsr	r5
	mflr	r6
	mfcr	r7
#ifdef RTEMS_MULTIPROCESSING
	/*
	 * We have to clear the reservation of the executing thread.  See also
	 * Book E section 6.1.6.2 "Atomic Update Primitives".
	 *
	 * Here we assume PPC_CONTEXT_OFFSET_SP == PPC_CONTEXT_CACHE_LINE_0.
	 */
	stwcx.	r1, r3, r10
#endif
	stw	r1, PPC_CONTEXT_OFFSET_SP(r3)
	stw	r5, PPC_CONTEXT_OFFSET_MSR(r3)
	stw	r6, PPC_CONTEXT_OFFSET_LR(r3)
	stw	r7, PPC_CONTEXT_OFFSET_CR(r3)
	evstdd	r14, PPC_CONTEXT_OFFSET_GPR14(r3)
	evstdd	r15, PPC_CONTEXT_OFFSET_GPR15(r3)

	DATA_CACHE_ZERO_AND_TOUCH(r10, PPC_CONTEXT_CACHE_LINE_2)

	evstdd	r16, PPC_CONTEXT_OFFSET_GPR16(r3)
	evstdd	r17, PPC_CONTEXT_OFFSET_GPR17(r3)
	evstdd	r18, PPC_CONTEXT_OFFSET_GPR18(r3)
	evstdd	r19, PPC_CONTEXT_OFFSET_GPR19(r3)

	DATA_CACHE_ZERO_AND_TOUCH(r10, PPC_CONTEXT_CACHE_LINE_3)

	evstdd	r20, PPC_CONTEXT_OFFSET_GPR20(r3)
	evstdd	r21, PPC_CONTEXT_OFFSET_GPR21(r3)
	evstdd	r22, PPC_CONTEXT_OFFSET_GPR22(r3)
	evstdd	r23, PPC_CONTEXT_OFFSET_GPR23(r3)

	DATA_CACHE_ZERO_AND_TOUCH(r10, PPC_CONTEXT_CACHE_LINE_4)

	evstdd	r24, PPC_CONTEXT_OFFSET_GPR24(r3)
	evstdd	r25, PPC_CONTEXT_OFFSET_GPR25(r3)
	evstdd	r26, PPC_CONTEXT_OFFSET_GPR26(r3)
	evstdd	r27, PPC_CONTEXT_OFFSET_GPR27(r3)

	evstdd	r28, PPC_CONTEXT_OFFSET_GPR28(r3)
	evstdd	r29, PPC_CONTEXT_OFFSET_GPR29(r3)
	evstdd	r30, PPC_CONTEXT_OFFSET_GPR30(r3)
	evstdd	r31, PPC_CONTEXT_OFFSET_GPR31(r3)

	/* Restore context from r4 */
restore_context:

	lwz	r1, PPC_CONTEXT_OFFSET_SP(r4)
	lwz	r5, PPC_CONTEXT_OFFSET_MSR(r4)
	lwz	r6, PPC_CONTEXT_OFFSET_LR(r4)
	lwz	r7, PPC_CONTEXT_OFFSET_CR(r4)

	evldd	r14, PPC_CONTEXT_OFFSET_GPR14(r4)
	evldd	r15, PPC_CONTEXT_OFFSET_GPR15(r4)

	DATA_CACHE_TOUCH(r0, r1)

	evldd	r16, PPC_CONTEXT_OFFSET_GPR16(r4)
	evldd	r17, PPC_CONTEXT_OFFSET_GPR17(r4)
	evldd	r18, PPC_CONTEXT_OFFSET_GPR18(r4)
	evldd	r19, PPC_CONTEXT_OFFSET_GPR19(r4)

	evldd	r20, PPC_CONTEXT_OFFSET_GPR20(r4)
	evldd	r21, PPC_CONTEXT_OFFSET_GPR21(r4)
	evldd	r22, PPC_CONTEXT_OFFSET_GPR22(r4)
	evldd	r23, PPC_CONTEXT_OFFSET_GPR23(r4)

	evldd	r24, PPC_CONTEXT_OFFSET_GPR24(r4)
	evldd	r25, PPC_CONTEXT_OFFSET_GPR25(r4)
	evldd	r26, PPC_CONTEXT_OFFSET_GPR26(r4)
	evldd	r27, PPC_CONTEXT_OFFSET_GPR27(r4)

	evldd	r28, PPC_CONTEXT_OFFSET_GPR28(r4)
	evldd	r29, PPC_CONTEXT_OFFSET_GPR29(r4)
	evldd	r30, PPC_CONTEXT_OFFSET_GPR30(r4)
	evldd	r31, PPC_CONTEXT_OFFSET_GPR31(r4)

	mtcr	r7
	mtlr	r6
	mtmsr	r5

	blr
#endif /* __SPE__ */

/*
 *  _CPU_Context_restore
 *
 *  This routine is generallu used only to restart self in an
 *  efficient manner.  It may simply be a label in _CPU_Context_switch.
 *
 *  NOTE: May be unnecessary to reload some registers.
 */
/*
 * ACB: Don't worry about cache optimisation here - this is not THAT critical.
 */
	ALIGN (PPC_CACHE_ALIGNMENT, PPC_CACHE_ALIGN_POWER)
	PUBLIC_PROC (_CPU_Context_restore)
PROC (_CPU_Context_restore):
#ifndef __SPE__
	lwz	r5, GP_CR(r3)
	lwz	r6, GP_PC(r3)
	lwz	r7, GP_MSR(r3)
	mtcrf	255, r5
	mtlr	r6
	mtmsr	r7
	isync
	lwz	r1, GP_1(r3)
	lwz	r2, GP_2(r3)
#if (PPC_USE_MULTIPLE == 1)
	lmw	r13, GP_13(r3)
#else
	lwz	r13, GP_13(r3)
	lwz	r14, GP_14(r3)
	lwz	r15, GP_15(r3)
	lwz	r16, GP_16(r3)
	lwz	r17, GP_17(r3)
	lwz	r18, GP_18(r3)
	lwz	r19, GP_19(r3)
	lwz	r20, GP_20(r3)
	lwz	r21, GP_21(r3)
	lwz	r22, GP_22(r3)
	lwz	r23, GP_23(r3)
	lwz	r24, GP_24(r3)
	lwz	r25, GP_25(r3)
	lwz	r26, GP_26(r3)
	lwz	r27, GP_27(r3)
	lwz	r28, GP_28(r3)
	lwz	r29, GP_29(r3)
	lwz	r30, GP_30(r3)
	lwz	r31, GP_31(r3)
#endif
#ifdef __ALTIVEC__
	EXTERN_PROC(_CPU_Context_restore_altivec)
	b _CPU_Context_restore_altivec
#endif
	blr
#else /* __SPE__ */
	/* Align to a cache line */
	clrrwi	r4, r3, 5

	b	restore_context
#endif /* __SPE__ */
