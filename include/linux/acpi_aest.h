/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ACPI_AEST_H__
#define __ACPI_AEST_H__

#include <asm/ras.h>
#include <linux/acpi.h>

/* AEST resource name */
#define AEST_NODE_NAME "AEST:NODE"
#define AEST_FHI_NAME "AEST:FHI"
#define AEST_ERI_NAME "AEST:ERI"

/* AEST interrupt */
#define AEST_INTERRUPT_MODE BIT(0)

#define AEST_MAX_INTERRUPT_PER_NODE 2

#define KB 1024
#define MB (1024 * KB)
#define GB (1024 * MB)

struct aest_hnode {
	struct list_head list;
	int count;
	u32 id;
	int type;
};

struct acpi_aest_node {
	struct list_head list;
	int type;
	struct acpi_aest_node_interface_header *interface_hdr;
	unsigned long *record_implemented;
	unsigned long *status_reporting;
	unsigned long *addressing_mode;
	struct acpi_aest_node_interface_common *common;
	union {
		struct acpi_aest_processor *processor;
		struct acpi_aest_memory *memory;
		struct acpi_aest_smmu *smmu;
		struct acpi_aest_vendor_v2 *vendor;
		struct acpi_aest_gic *gic;
		struct acpi_aest_pcie *pcie;
		struct acpi_aest_proxy *proxy;
		void *spec_pointer;
	};
	union {
		struct acpi_aest_processor_cache *cache;
		struct acpi_aest_processor_tlb *tlb;
		struct acpi_aest_processor_generic *generic;
		void *processor_spec_pointer;
	};
	struct acpi_aest_node_interrupt_v2 *interrupt;
	int interrupt_count;
};
#endif /* __ACPI_AEST_H__ */
