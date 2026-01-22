// SPDX-License-Identifier: GPL-2.0
/*
 * ARM Error Source Table Support
 *
 * Copyright (c) 2025, Alibaba Group.
 */

#include "aest.h"

/*******************************************************************************
 *
 * Attribute for AEST record
 *
 ******************************************************************************/

#define DEFINE_AEST_DEBUGFS_ATTR(name, offset) \
static int name##_get(void *data, u64 *val) \
{ \
	struct aest_record *record = data; \
	*val = record_read(record, offset); \
	return 0; \
} \
static int name##_set(void *data, u64 val) \
{ \
	struct aest_record *record = data; \
	record_write(record, offset, val); \
	return 0; \
} \
DEFINE_DEBUGFS_ATTRIBUTE(name##_ops, name##_get, name##_set, "%#llx\n")

DEFINE_AEST_DEBUGFS_ATTR(err_fr, ERXFR);
DEFINE_AEST_DEBUGFS_ATTR(err_ctrl, ERXCTLR);
DEFINE_AEST_DEBUGFS_ATTR(err_status, ERXSTATUS);
DEFINE_AEST_DEBUGFS_ATTR(err_addr, ERXADDR);
DEFINE_AEST_DEBUGFS_ATTR(err_misc0, ERXMISC0);
DEFINE_AEST_DEBUGFS_ATTR(err_misc1, ERXMISC1);
DEFINE_AEST_DEBUGFS_ATTR(err_misc2, ERXMISC2);
DEFINE_AEST_DEBUGFS_ATTR(err_misc3, ERXMISC3);

static void aest_record_init_debugfs(struct aest_record *record)
{
	debugfs_create_file("err_fr", 0600, record->debugfs, record,
								&err_fr_ops);
	debugfs_create_file("err_ctrl", 0600, record->debugfs, record,
								&err_ctrl_ops);
	debugfs_create_file("err_status", 0600, record->debugfs, record,
								&err_status_ops);
	debugfs_create_file("err_addr", 0600, record->debugfs, record,
								&err_addr_ops);
	debugfs_create_file("err_misc0", 0600, record->debugfs, record,
								&err_misc0_ops);
	debugfs_create_file("err_misc1", 0600, record->debugfs, record,
								&err_misc1_ops);
	debugfs_create_file("err_misc2", 0600, record->debugfs, record,
								&err_misc2_ops);
	debugfs_create_file("err_misc3", 0600, record->debugfs, record,
								&err_misc3_ops);
}

static void
aest_node_init_debugfs(struct aest_node *node)
{
	int i;
	struct aest_record *record;

	for (i = 0; i < node->record_count; i++) {
		record = &node->records[i];
		if (!record->name)
			continue;
		record->debugfs = debugfs_create_dir(record->name,
								node->debugfs);
		aest_record_init_debugfs(record);
	}
}

static void
aest_oncore_dev_init_debugfs(struct aest_device *adev)
{
	int cpu, i;
	struct aest_node *node;
	struct aest_device *percpu_dev;
	char name[16];

	for_each_possible_cpu(cpu) {
		percpu_dev = this_cpu_ptr(adev->adev_oncore);

		snprintf(name, sizeof(name), "processor%u", cpu);
		percpu_dev->debugfs = debugfs_create_dir(name, aest_debugfs);

		for (i = 0; i < adev->node_cnt; i++) {
			node = &adev->nodes[i];

			node->debugfs = debugfs_create_dir(node->name,
							percpu_dev->debugfs);
			aest_node_init_debugfs(node);
		}
	}
}

void aest_dev_init_debugfs(struct aest_device *adev)
{
	int i;
	struct aest_node *node;

	adev->debugfs = debugfs_create_dir(dev_name(adev->dev), aest_debugfs);
	if (aest_dev_is_oncore(adev)) {
		aest_oncore_dev_init_debugfs(adev);
		return;
	}

	for (i = 0; i < adev->node_cnt; i++) {
		node = &adev->nodes[i];
		if (!node->name)
			continue;
		node->debugfs = debugfs_create_dir(node->name, adev->debugfs);
		aest_node_init_debugfs(node);
	}
}
