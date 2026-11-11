/* AI GENERATED TEST */
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

/* 3 frames: three different frames come out, all aligned and inside RAM, then KERR_NOMEM. */
static void test_alloc_until_empty(void)
{
    uint64_t storage[1] = {0};
    phys_addr_t got[3] = {{0}, {0}, {0}};
    phys_addr_t untouched = {0x1234};

    expect("alloc setup", pmm_init(phys(RAM_BASE), 3ULL * FRAME, storage, 8) == KERR_OK);
    for (size_t i = 0; i < 3; i++) {
        expect("alloc: KERR_OK", pmm_alloc_frame(&got[i]) == KERR_OK);
        expect("alloc: aligned", (got[i].value & (FRAME - 1U)) == 0U);
        expect("alloc: inside RAM",
               got[i].value >= RAM_BASE && got[i].value < RAM_BASE + 3ULL * FRAME);
        expect("alloc: free count drops", pmm_free_count() == 2U - i);
    }
    expect("alloc: all different", got[0].value != got[1].value && got[0].value != got[2].value &&
                                       got[1].value != got[2].value);

    expect("empty: KERR_NOMEM", pmm_alloc_frame(&untouched) == KERR_NOMEM);
    expect("empty: out param untouched", untouched.value == 0x1234U);
    expect("empty: free count stays 0", pmm_free_count() == 0U);
}

/*
 * 100 frames: the bitmap's last word has 28 bits past the end of RAM. Exactly 100 frames must
 * come out, none of them past the end.
 */
static void test_alloc_never_past_ram(void)
{
    uint64_t storage[2] = {0, 0};
    phys_addr_t frame = {0};
    size_t count = 0;
    int all_inside = 1;

    expect("tail setup", pmm_init(phys(RAM_BASE), 100ULL * FRAME, storage, 16) == KERR_OK);
    while (count < 200 && pmm_alloc_frame(&frame) == KERR_OK) {
        if (frame.value >= RAM_BASE + 100ULL * FRAME) {
            all_inside = 0;
        }
        count++;
    }
    expect("tail: exactly 100 frames handed out", count == 100U);
    expect("tail: none past the end of RAM", all_inside);
}

/* A freed frame can be taken again, and the free count goes back up. */
static void test_free_then_reuse(void)
{
    uint64_t storage[1] = {0};
    phys_addr_t a = {0};
    phys_addr_t b = {0};
    phys_addr_t again = {0};

    expect("reuse setup", pmm_init(phys(RAM_BASE), 2ULL * FRAME, storage, 8) == KERR_OK);
    expect("reuse: alloc a", pmm_alloc_frame(&a) == KERR_OK);
    expect("reuse: alloc b", pmm_alloc_frame(&b) == KERR_OK);

    expect("free a: KERR_OK", pmm_free_frame(a) == KERR_OK);
    expect("free a: count 1", pmm_free_count() == 1U);
    expect("realloc: KERR_OK", pmm_alloc_frame(&again) == KERR_OK);
    expect("realloc: gets a back", again.value == a.value);
    expect("realloc: count 0", pmm_free_count() == 0U);
}

/* Every bad free returns KERR_INVALID and changes nothing. */
static void test_free_rejects(void)
{
    uint64_t storage[1] = {0};
    phys_addr_t a = {0};
    const uint64_t ram_end = RAM_BASE + 4ULL * FRAME;

    expect("reject setup", pmm_init(phys(RAM_BASE), 4ULL * FRAME, storage, 8) == KERR_OK);
    expect("reject: alloc", pmm_alloc_frame(&a) == KERR_OK);

    expect("free never allocated", pmm_free_frame(phys(RAM_BASE + FRAME)) == KERR_INVALID);
    expect("free unaligned", pmm_free_frame(phys(a.value + 8U)) == KERR_INVALID);
    expect("free below RAM", pmm_free_frame(phys(RAM_BASE - FRAME)) == KERR_INVALID);
    expect("free address 0", pmm_free_frame(phys(0)) == KERR_INVALID);
    expect("free at RAM end", pmm_free_frame(phys(ram_end)) == KERR_INVALID);
    expect("free far past RAM", pmm_free_frame(phys(0xFFFFFFFFFFFFF000ULL)) == KERR_INVALID);
    expect("rejects: count unchanged", pmm_free_count() == 3U);

    expect("free a: KERR_OK", pmm_free_frame(a) == KERR_OK);
    expect("double free: KERR_INVALID", pmm_free_frame(a) == KERR_INVALID);
    expect("double free: count unchanged", pmm_free_count() == 4U);
}

/* Frames handed out until empty, as a bit mask (bit i = frame i came out). RAM must be small. */
static uint64_t alloc_all_mask(void)
{
    phys_addr_t frame = {0};
    uint64_t mask = 0;
    size_t guard = 0;

    while (guard < 64 && pmm_alloc_frame(&frame) == KERR_OK) {
        mask |= 1ULL << ((frame.value - RAM_BASE) / FRAME);
        guard++;
    }
    return mask;
}

/* 4 frames. Every bad range returns KERR_INVALID and reserves nothing. */
static void test_reserve_rejects(void)
{
    uint64_t storage[1] = {0};
    const uint64_t ram_end = RAM_BASE + 4ULL * FRAME;

    expect("reserve reject setup", pmm_init(phys(RAM_BASE), 4ULL * FRAME, storage, 8) == KERR_OK);

    expect("reserve size 0", pmm_reserve(phys(RAM_BASE), 0) == KERR_INVALID);
    expect("reserve base + size overflows",
           pmm_reserve(phys(0xFFFFFFFFFFFFF000ULL), 2ULL * FRAME) == KERR_INVALID);
    expect("reserve below RAM", pmm_reserve(phys(RAM_BASE - FRAME), FRAME) == KERR_INVALID);
    expect("reserve straddles RAM start", pmm_reserve(phys(RAM_BASE - 8U), 16) == KERR_INVALID);
    expect("reserve straddles RAM end",
           pmm_reserve(phys(ram_end - FRAME), 2ULL * FRAME) == KERR_INVALID);
    expect("reserve starts at RAM end", pmm_reserve(phys(ram_end), 1) == KERR_INVALID);
    expect("reserve far past RAM", pmm_reserve(phys(0xFFFFFFFFFFFFF000ULL), 1) == KERR_INVALID);

    expect("reserve rejects: count unchanged", pmm_free_count() == 4U);
    expect("reserve rejects: nothing marked", alloc_all_mask() == 0xFU);
}

/* The bytes past the last whole frame are not managed, so they cannot be reserved. */
static void test_reserve_partial_tail(void)
{
    uint64_t storage[1] = {0};

    expect("tail reserve setup",
           pmm_init(phys(RAM_BASE), 4ULL * FRAME + 100U, storage, 8) == KERR_OK);
    expect("reserve in untracked tail",
           pmm_reserve(phys(RAM_BASE + 4ULL * FRAME), 50) == KERR_INVALID);
    expect("reserve tail: count unchanged", pmm_free_count() == 4U);
}

/* Each case starts from 8 free frames and checks exactly which frames end up reserved. */
static void test_reserve_rounding(void)
{
    static const struct {
        const char *name;
        uint64_t offset;
        uint64_t size;
        uint64_t reserved;
    } cases[] = {
        {"reserve exactly frame 1", FRAME, FRAME, 0x02U},
        {"reserve frames 0-1, end on boundary", 0, 2ULL * FRAME, 0x03U},
        {"reserve 1 byte", 3ULL * FRAME + 7U, 1, 0x08U},
        {"reserve inside one frame", FRAME + 100U, 10, 0x02U},
        {"reserve 2 bytes across a boundary", FRAME - 1U, 2, 0x03U},
        {"reserve unaligned span of 3 frames", FRAME + 1U, 2ULL * FRAME, 0x0EU},
        {"reserve last byte of RAM", 8ULL * FRAME - 1U, 1, 0x80U},
        {"reserve all of RAM", 0, 8ULL * FRAME, 0xFFU},
    };
    uint64_t storage[1] = {0};

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const uint64_t want_free = 8U - (uint64_t)__builtin_popcountll(cases[i].reserved);

        expect(cases[i].name, pmm_init(phys(RAM_BASE), 8ULL * FRAME, storage, 8) == KERR_OK);
        expect(cases[i].name,
               pmm_reserve(phys(RAM_BASE + cases[i].offset), cases[i].size) == KERR_OK);
        expect(cases[i].name, pmm_free_count() == want_free);
        expect(cases[i].name, alloc_all_mask() == (0xFFU & ~cases[i].reserved));
    }
}

/* Reserving a frame that is already used must not count it twice. */
static void test_reserve_already_used(void)
{
    uint64_t storage[1] = {0};
    phys_addr_t a = {0};

    expect("overlap setup", pmm_init(phys(RAM_BASE), 4ULL * FRAME, storage, 8) == KERR_OK);
    expect("overlap: frames 0-1", pmm_reserve(phys(RAM_BASE), 2ULL * FRAME) == KERR_OK);
    expect("overlap: frames 1-2", pmm_reserve(phys(RAM_BASE + FRAME), 2ULL * FRAME) == KERR_OK);
    expect("overlap: 1 free, not 0", pmm_free_count() == 1U);
    expect("overlap: same range again", pmm_reserve(phys(RAM_BASE), 2ULL * FRAME) == KERR_OK);
    expect("overlap: still 1 free", pmm_free_count() == 1U);

    expect("allocated setup", pmm_init(phys(RAM_BASE), 4ULL * FRAME, storage, 8) == KERR_OK);
    expect("allocated: alloc a", pmm_alloc_frame(&a) == KERR_OK);
    expect("allocated: reserve a", pmm_reserve(a, FRAME) == KERR_OK);
    expect("allocated: count unchanged", pmm_free_count() == 3U);
}

int main(void)
{
    test_bitmap_bytes();
    test_init_rejects();
    test_init_clears();
    test_init_edges();
    test_alloc_until_empty();
    test_alloc_never_past_ram();
    test_free_then_reuse();
    test_free_rejects();
    test_reserve_rejects();
    test_reserve_partial_tail();
    test_reserve_rounding();
    test_reserve_already_used();

    if (failures != 0) {
        printf("pmm_test: %d failed\n", failures);
        return 1;
    }
    printf("PASS  pmm_test\n");
    return 0;
}
