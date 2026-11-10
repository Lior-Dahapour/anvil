/* Host unit tests for the physical memory manager (`make test`, ASan + UBSan). */
#include <anvil/panic.h>
#include <anvil/pmm.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define FRAME PMM_FRAME_SIZE
#define RAM_BASE 0x40000000ULL
#define FILL 0xA5A5A5A5A5A5A5A5ULL

static int failures;

/* KASSERT calls this. A failed assertion is a bug in the code under test: stop loudly. */
_Noreturn void kpanic(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printf("FAIL  kpanic: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    exit(1);
}

static void expect(const char *name, int ok)
{
    if (!ok) {
        printf("FAIL  %s\n", name);
        failures++;
    }
}

static phys_addr_t phys(uint64_t value)
{
    phys_addr_t p = {value};
    return p;
}

/* Garbage in the storage, so the tests see whether pmm_init really clears it. */
static void fill(uint64_t *storage, size_t words)
{
    for (size_t i = 0; i < words; i++) {
        storage[i] = FILL;
    }
}

static void test_bitmap_bytes(void)
{
    expect("bytes: 0 RAM -> 0", pmm_bitmap_bytes(0) == 0U);
    expect("bytes: under one frame -> 0", pmm_bitmap_bytes(FRAME - 1U) == 0U);
    expect("bytes: 1 frame -> 8", pmm_bitmap_bytes(FRAME) == 8U);
    expect("bytes: 64 frames -> 8", pmm_bitmap_bytes(64ULL * FRAME) == 8U);
    expect("bytes: 65 frames -> 16", pmm_bitmap_bytes(65ULL * FRAME) == 16U);
    expect("bytes: partial last frame ignored", pmm_bitmap_bytes(64ULL * FRAME + 100U) == 8U);
    expect("bytes: 128 MiB -> 4096", pmm_bitmap_bytes(128ULL << 20) == 4096U);
}

/* Every bad input returns KERR_INVALID and leaves the previous state alone. */
static void test_init_rejects(void)
{
    uint64_t good[2] = {0, 0};
    uint64_t storage[2] = {0, 0};
    const uint64_t size = 100ULL * FRAME;

    expect("setup: valid init", pmm_init(phys(RAM_BASE), size, good, sizeof(good)) == KERR_OK);
    fill(storage, 2);

    expect("NULL storage", pmm_init(phys(RAM_BASE), size, NULL, 16) == KERR_INVALID);
    expect("unaligned base",
           pmm_init(phys(RAM_BASE + 0x800U), size, storage, sizeof(storage)) == KERR_INVALID);
    expect("size 0", pmm_init(phys(RAM_BASE), 0, storage, sizeof(storage)) == KERR_INVALID);
    expect("size under one frame",
           pmm_init(phys(RAM_BASE), FRAME - 1U, storage, sizeof(storage)) == KERR_INVALID);
    expect("base + size overflows", pmm_init(phys(0xFFFFFFFFFFFFF000ULL), 2ULL * FRAME, storage,
                                             sizeof(storage)) == KERR_INVALID);
    expect("storage one word too small",
           pmm_init(phys(RAM_BASE), size, storage, 8) == KERR_INVALID);

    expect("rejects keep free count", pmm_free_count() == 100U);
    expect("rejects leave storage untouched", storage[0] == FILL && storage[1] == FILL);
}

/* 100 frames need 2 words; a third word is given but must not be touched. */
static void test_init_clears(void)
{
    uint64_t storage[3] = {0, 0, 0};
    fill(storage, 3);

    expect("100 frames: KERR_OK",
           pmm_init(phys(RAM_BASE), 100ULL * FRAME, storage, sizeof(storage)) == KERR_OK);
    expect("100 frames: all free", pmm_free_count() == 100U);
    expect("100 frames: needed words cleared", storage[0] == 0U && storage[1] == 0U);
    expect("100 frames: spare word untouched", storage[2] == FILL);
}

static void test_init_edges(void)
{
    uint64_t one[1] = {0};
    static uint64_t big[512];

    expect("1 frame: KERR_OK", pmm_init(phys(RAM_BASE), FRAME, one, sizeof(one)) == KERR_OK);
    expect("1 frame: 1 free", pmm_free_count() == 1U);

    expect("partial frame: KERR_OK",
           pmm_init(phys(RAM_BASE), 64ULL * FRAME + 123U, one, sizeof(one)) == KERR_OK);
    expect("partial frame: ignored", pmm_free_count() == 64U);

    expect("top of address space: KERR_OK",
           pmm_init(phys(0xFFFFFFFFFFFFE000ULL), FRAME, one, sizeof(one)) == KERR_OK);

    fill(big, 512);
    expect("128 MiB: KERR_OK", pmm_init(phys(RAM_BASE), 128ULL << 20, big, sizeof(big)) == KERR_OK);
    expect("128 MiB: 32768 free", pmm_free_count() == 32768U);
    expect("128 MiB: last word cleared", big[511] == 0U);
}

int main(void)
{
    test_bitmap_bytes();
    test_init_rejects();
    test_init_clears();
    test_init_edges();

    if (failures != 0) {
        printf("pmm_test: %d failed\n", failures);
        return 1;
    }
    printf("PASS  pmm_test\n");
    return 0;
}
