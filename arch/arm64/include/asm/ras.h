/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_RAS_H
#define __ASM_RAS_H

#include <linux/types.h>

/* ERR<n>FR */
#define ERR_FR_CE GENMASK_ULL(54, 53)
#define ERR_FR_RP BIT(15)
#define ERR_FR_CEC GENMASK_ULL(14, 12)

#define ERR_FR_RP_SINGLE_COUNTER 0
#define ERR_FR_RP_DOUBLE_COUNTER 1

#define ERR_FR_CEC_0B_COUNTER 0
#define ERR_FR_CEC_8B_COUNTER BIT(1)
#define ERR_FR_CEC_16B_COUNTER BIT(2)

/* ERR<n>MISC0 */

/* ERR<n>FR.CEC == 0b010, ERR<n>FR.RP == 0  */
#define ERR_MISC0_8B_OF BIT(39)
#define ERR_MISC0_8B_CEC GENMASK_ULL(38, 32)

/* ERR<n>FR.CEC == 0b100, ERR<n>FR.RP == 0  */
#define ERR_MISC0_16B_OF BIT(47)
#define ERR_MISC0_16B_CEC GENMASK_ULL(46, 32)

#define ERR_MISC0_CEC_SHIFT 31

#define ERR_8B_CEC_MAX (ERR_MISC0_8B_CEC >> ERR_MISC0_CEC_SHIFT)
#define ERR_16B_CEC_MAX (ERR_MISC0_16B_CEC >> ERR_MISC0_CEC_SHIFT)

/* ERR<n>FR.CEC == 0b100, ERR<n>FR.RP == 1  */
#define ERR_MISC0_16B_OFO BIT(63)
#define ERR_MISC0_16B_CECO GENMASK_ULL(62, 48)
#define ERR_MISC0_16B_OFR BIT(47)
#define ERR_MISC0_16B_CECR GENMASK_ULL(46, 32)

/* ERRDEVARCH */
#define ERRDEVARCH_REV GENMASK(19, 16)

enum ras_ce_threshold {
	RAS_CE_THRESHOLD_0B,
	RAS_CE_THRESHOLD_8B,
	RAS_CE_THRESHOLD_16B,
	RAS_CE_THRESHOLD_32B,
	UNKNOWN,
};

struct ras_ext_regs {
	u64 err_fr;
	u64 err_ctlr;
	u64 err_status;
	u64 err_addr;
	u64 err_misc[4];
};

#endif /* __ASM_RAS_H */
