/* SPDX-License-Identifier: GPL-2.0 */
/*
 * DMABUF Heaps Allocation Infrastructure
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2019 Linaro Ltd.
 */

#ifndef _DMA_HEAPS_H
#define _DMA_HEAPS_H

#include <linux/types.h>

struct cma;
struct dma_heap;

/**
 * struct dma_heap_ops - ops to operate on a given heap
 * @allocate:	allocate dmabuf and return struct dma_buf ptr
 *
 * allocate returns dmabuf on success, ERR_PTR(-errno) on error.
 */
struct dma_heap_ops {
	struct dma_buf *(*allocate)(struct dma_heap *heap,
				    unsigned long len,
				    u32 fd_flags,
				    u64 heap_flags);
};

/**
 * struct dma_heap_export_info - information needed to export a new dmabuf heap
 * @name:	used for debugging/device-node name
 * @ops:	ops struct for this heap
 * @priv:	heap exporter private data
 *
 * Information needed to export a new dmabuf heap.
 */
struct dma_heap_export_info {
	const char *name;
	const struct dma_heap_ops *ops;
	void *priv;
};

void *dma_heap_get_drvdata(struct dma_heap *heap);

const char *dma_heap_get_name(struct dma_heap *heap);

struct dma_heap *dma_heap_add(const struct dma_heap_export_info *exp_info);

/**
 * dma_heap_find - get the heap registered with the specified name
 * @name: Name of the DMA-Heap to find
 *
 * Returns:
 * The DMA-Heap with the provided name.
 *
 * NOTE: DMA-Heaps returned from this function MUST be released using
 * dma_heap_put() when the user is done to enable the heap to be unloaded.
 */
struct dma_heap *dma_heap_find(const char *name);

/**
 * dma_heap_put - drops a reference to a dmabuf heap, potentially freeing it
 * @heap: the heap whose reference count to decrement
 */
void dma_heap_put(struct dma_heap *heap);

/**
 * dma_heap_buffer_alloc - Allocate dma-buf from a dma_heap
 * @heap:	DMA-Heap to allocate from
 * @len:	size to allocate in bytes
 * @fd_flags:	flags to set on returned dma-buf fd
 * @heap_flags: flags to pass to the dma heap
 *
 * This is for internal dma-buf allocations only. Free returned buffers with dma_buf_put().
 */
struct dma_buf *dma_heap_buffer_alloc(struct dma_heap *heap, size_t len,
				      unsigned int fd_flags,
				      unsigned int heap_flags);

#ifdef CONFIG_DMABUF_HEAPS_CMA
int cma_heap_add(struct cma *cma, void *data);
#else
static inline int cma_heap_add(struct cma *cma, void *data)
{
	return -EINVAL;
}
#endif /* CONFIG_DMABUF_HEAPS_CMA */

#endif /* _DMA_HEAPS_H */
