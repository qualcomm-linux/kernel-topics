/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _MEMORY_DUMP_RESERVE_H
#define _MEMORY_DUMP_RESERVE_H
#include <linux/cma.h>

void __init reserve_memdump_cma(void);
extern struct cma *memdump_cma;

#endif
