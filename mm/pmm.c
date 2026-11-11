//
// Created by Lior Dahapour on 10/11/2026.
//
#include <anvil/bitmap.h>
#include <anvil/kerr.h>
#include <anvil/panic.h>
#include <anvil/pmm.h>
#include <anvil/types.h>
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
    if (size == 0) {
        return KERR_INVALID;
    }
    uint64_t end = 0;
    if (__builtin_add_overflow(base.value, size, &end)) {
        return KERR_INVALID;
    }
    // end need to be inside the ram range , while also making sure, the 'base' is in the ram
    if (!(base.value >= pmm.ram_base && end <= pmm.ram_base + pmm.frame_count * PMM_FRAME_SIZE)) {
        return KERR_INVALID;
    }
    const size_t start_frame = (base.value - pmm.ram_base) / PMM_FRAME_SIZE; // round down
    const size_t end_frame = (end - pmm.ram_base + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE; // round up
    for (size_t i = start_frame; i < end_frame; i++) {
        if (bitmap_test(&pmm.frames, i)) {
            continue;
        }
        bitmap_set(&pmm.frames, i);
        pmm.free_count--;
    }
    return KERR_OK;
}

KMUST_CHECK kerr_t pmm_alloc_frame(phys_addr_t *frame)
{
    KASSERT(frame != NULL);
    size_t bit = 0;
    if (bitmap_find_zero(&pmm.frames, &bit) != KERR_OK) {
        return KERR_NOMEM;
    }
    bitmap_set(&pmm.frames, bit);
    pmm.free_count--;
    // SAFETY: frame non-null (KASSERT), caller passes a valid out pointer.
    frame->value = pmm.ram_base + bit * PMM_FRAME_SIZE;
    return KERR_OK;
}

KMUST_CHECK kerr_t pmm_free_frame(const phys_addr_t frame)
{
    if (pmm.ram_base > frame.value) {
        return KERR_INVALID;
    }
    if ((frame.value & (PMM_FRAME_SIZE - 1)) != 0) {
        // frame isn't aligned
        return KERR_INVALID;
    }
    size_t nframe = (frame.value - pmm.ram_base) / PMM_FRAME_SIZE;
    if (nframe >= pmm.frame_count) {
        return KERR_INVALID;
    }
    if (!bitmap_test(&pmm.frames, nframe)) {
        // already free
        return KERR_INVALID;
    }
    bitmap_clear(&pmm.frames, nframe);
    pmm.free_count++;
    return KERR_OK;
}

size_t pmm_free_count(void)
{
    return pmm.free_count;
}
