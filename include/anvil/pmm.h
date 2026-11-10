#ifndef ANVIL_PMM_H
#define ANVIL_PMM_H

#include <anvil/kerr.h>
#include <anvil/types.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Physical memory manager: hands out 4 KiB frames of RAM, tracked in a bitmap (1 bit per
 * frame, 1 = used). One RAM range, single core, no lock yet: every function has
 * Context: early boot, single core.
 */

#define PMM_FRAME_SIZE 4096U

/**
 * Bytes of bitmap storage pmm_init needs for `ram_size` bytes of RAM. A multiple of 8.
 *
 * Context: any.
 */
size_t pmm_bitmap_bytes(uint64_t ram_size);

/**
 * Start tracking one RAM range, with every frame free. A partial frame at the end is ignored.
 *
 * @param ram_base      Start of RAM. Must be frame-aligned.
 * @param ram_size      Bytes of RAM. At least one frame.
 * @param bitmap        Storage for the bitmap, 8-byte aligned, at least
 *                      pmm_bitmap_bytes(ram_size) bytes. Owned by the PMM from now on and must
 *                      stay mapped forever. The caller must also reserve it if it lies in RAM.
 * @param bitmap_bytes  Size of `bitmap` in bytes.
 *
 * @return KERR_OK, or KERR_INVALID if `bitmap` is NULL or too small, `ram_base` is not
 *         aligned, or `ram_size` is less than one frame.
 *
 * Context: early boot, single core. Calling it again starts over.
 */
KMUST_CHECK kerr_t pmm_init(phys_addr_t ram_base, uint64_t ram_size, uint64_t *bitmap,
                            size_t bitmap_bytes);

/**
 * Mark every frame touched by [base, base + size) as used, so it is never handed out.
 * Rounds outward to whole frames. Reserving an already used frame is fine.
 *
 * @return KERR_OK, or KERR_INVALID if `size` is 0, `base + size` overflows, or the range is
 *         not entirely inside RAM.
 *
 * Context: early boot, single core, after pmm_init.
 */
KMUST_CHECK kerr_t pmm_reserve(phys_addr_t base, uint64_t size);

/**
 * Take one free frame. Its contents are not cleared.
 *
 * @param frame  Out: the frame's physical address, frame-aligned. Must not be NULL.
 *
 * @return KERR_OK, or KERR_NOMEM if no frame is free.
 *
 * Context: early boot, single core, after pmm_init.
 */
KMUST_CHECK kerr_t pmm_alloc_frame(phys_addr_t *frame);

/**
 * Give a frame back.
 *
 * @param frame  A frame returned by pmm_alloc_frame and not freed since.
 *
 * @return KERR_OK, or KERR_INVALID if `frame` is unaligned, outside RAM, or already free
 *         (a double free: the caller should panic, it is a kernel bug).
 *
 * Context: early boot, single core, after pmm_init.
 */
KMUST_CHECK kerr_t pmm_free_frame(phys_addr_t frame);

/**
 * Number of free frames right now.
 *
 * Context: early boot, single core, after pmm_init.
 */
size_t pmm_free_count(void);

#endif
