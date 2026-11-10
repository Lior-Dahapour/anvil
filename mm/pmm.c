//
// Created by Lior Dahapour on 10/11/2026.
//
#include <anvil/bitmap.h>
#include <anvil/kerr.h>
#include <anvil/panic.h>
#include <anvil/pmm.h>
#include <anvil/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct pmm {
    uint64_t ram_base;
    size_t frame_count;
    size_t free_count;
    struct bitmap frames;
};

static struct pmm pmm = {0};

size_t pmm_bitmap_bytes(const uint64_t ram_size)
{
    const size_t frames = ram_size / PMM_FRAME_SIZE;
    // how many "space" we need to store the bitmap.word for this ram size
    return bitmap_words(frames) * sizeof(uint64_t);
}

KMUST_CHECK kerr_t pmm_init(const phys_addr_t ram_base, const uint64_t ram_size, uint64_t *bitmap,
                            const size_t bitmap_bytes)
{
    if (bitmap == NULL) {
        return KERR_INVALID;
    }

    if (ram_size < PMM_FRAME_SIZE) {
        // the ram can't hold single frame , it's too small :(
        return KERR_INVALID;
    }

    if ((ram_base.value & (PMM_FRAME_SIZE - 1)) != 0) {
        // the ram base isn't frame aligned
        return KERR_INVALID;
    }
    uint64_t ram_end = 0;
    if (__builtin_add_overflow(ram_base.value, ram_size, &ram_end)) {
        // ram overflow
        return KERR_INVALID;
    }

    if (bitmap_bytes < pmm_bitmap_bytes(ram_size)) {
        // bitmap size is too small to hold the pmm pages
        return KERR_INVALID;
    }
    pmm.ram_base = ram_base.value;
    pmm.frame_count = (size_t)ram_size / PMM_FRAME_SIZE;
    pmm.free_count = pmm.frame_count;
    pmm.frames.word = bitmap;
    pmm.frames.nbits = pmm.frame_count;
    for (size_t i = 0; i < bitmap_words(pmm.frames.nbits); i++) {
        // SAFETY: size check above + caller: storage has >= bitmap_words(nbits) words.
        pmm.frames.word[i] = 0;
    }
    return KERR_OK;
}

KMUST_CHECK kerr_t pmm_reserve(const phys_addr_t base, const uint64_t size)
{
    (void)base;
    (void)size;
    return KERR_INVALID;
}

KMUST_CHECK kerr_t pmm_alloc_frame(phys_addr_t *frame)
{
    (void)frame;
    return KERR_NOMEM;
}

KMUST_CHECK kerr_t pmm_free_frame(const phys_addr_t frame)
{
    (void)frame;
    return KERR_INVALID;
}

size_t pmm_free_count(void)
{
    return pmm.free_count;
}
