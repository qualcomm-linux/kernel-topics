/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _ASM_RISCV_CSR_INDIRECT_H
#define _ASM_RISCV_CSR_INDIRECT_H

#include <linux/types.h>
#include <linux/irqflags.h>

#include <asm/csr.h>

static inline unsigned long csr_indirect_read(u16 iregcsr, u32 iselbase, u32 iseloff)
{
	unsigned long __value = 0;
	unsigned long __flags;

	local_irq_save(__flags);
	csr_write(CSR_ISELECT, iselbase + iseloff);
	__value = csr_read(iregcsr);
	local_irq_restore(__flags);

	return __value;
}

static inline void csr_indirect_write(u16 iregcsr, u32 iselbase, u32 iseloff, unsigned long value)
{
	unsigned long __flags;

	local_irq_save(__flags);
	csr_write(CSR_ISELECT, iselbase + iseloff);
	csr_write(iregcsr, (value));
	local_irq_restore(__flags);
}

static inline unsigned long csr_indirect_warl(u16 iregcsr, u32 iselbase, u32 iseloff,
					      unsigned long warl_val)
{
	unsigned long __old_val = 0, __value = 0;
	unsigned long __flags;

	local_irq_save(__flags);
	csr_write(CSR_ISELECT, iselbase + iseloff);
	__old_val = csr_read(iregcsr);
	csr_write(iregcsr, warl_val);
	__value = csr_read(iregcsr);
	csr_write(iregcsr, __old_val);
	local_irq_restore(__flags);

	return __value;
}

#endif
