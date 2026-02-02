// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/cma.h>
#include <linux/memory_dump_reserve.h>

struct cma *memdump_cma;
#if IS_ENABLED(CONFIG_QCOM_MEMORY_DUMP_V2)
void __init reserve_memdump_cma(void)
{
	unsigned long long cma_size = 0x3000000;
	unsigned long long request_size = roundup(cma_size, PAGE_SIZE);

	if (cma_declare_contiguous(0, request_size, 0, 0, 0, false,
				   "memdump", &memdump_cma)) {
		pr_warn("memdump CMA reservation failed\n");
	}
}
#else
void __init reserve_memdump_cma()
{
	pr_warn("memdump CMA reservation not supported\n");
}
#endif
