/* AI GENERATED TEST */
/* Host unit tests for the bitmap helpers (`make test`, ASan + UBSan). */
#include <anvil/bitmap.h>
#include <anvil/panic.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

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

static void test_words(void)
{
    expect("words(0) == 0", bitmap_words(0) == 0U);
    expect("words(1) == 1", bitmap_words(1) == 1U);
    expect("words(64) == 1", bitmap_words(64) == 1U);
    expect("words(65) == 2", bitmap_words(65) == 2U);
    expect("words(32768) == 512", bitmap_words(32768) == 512U);
}

/* Each bit lands in the right word at the right position, and touches nothing else. */
static void test_set_clear(void)
{
    uint64_t storage[2] = {0, 0};
    struct bitmap b = {storage, 128};

    bitmap_set(&b, 70);
    expect("set 70: word 1 bit 6", storage[0] == 0U && storage[1] == (1ULL << 6));
    expect("test 70 after set", bitmap_test(&b, 70));
    expect("test 69 untouched", !bitmap_test(&b, 69));
    expect("test 6 untouched", !bitmap_test(&b, 6));

    bitmap_set(&b, 0);
    bitmap_set(&b, 63);
    bitmap_set(&b, 64);
    bitmap_set(&b, 127);
    expect("set edges",
           storage[0] == ((1ULL << 63) | 1U) && storage[1] == ((1ULL << 63) | (1ULL << 6) | 1U));

    bitmap_clear(&b, 70);
    expect("clear 70", !bitmap_test(&b, 70) && storage[1] == ((1ULL << 63) | 1U));
    bitmap_clear(&b, 70);
    expect("clear twice is harmless", storage[1] == ((1ULL << 63) | 1U));
}

static void test_find_zero(void)
{
    uint64_t storage[2] = {0, 0};
    struct bitmap b = {storage, 128};
    size_t bit = 999;

    expect("empty: finds 0", bitmap_find_zero(&b, &bit) == KERR_OK && bit == 0U);

    storage[0] = UINT64_MAX;
    expect("word 0 full: finds 64", bitmap_find_zero(&b, &bit) == KERR_OK && bit == 64U);

    storage[1] = UINT64_MAX & ~(1ULL << 9);
    expect("one hole: finds 73", bitmap_find_zero(&b, &bit) == KERR_OK && bit == 73U);

    storage[1] = UINT64_MAX;
    bit = 999;
    expect("full: not found", bitmap_find_zero(&b, &bit) == KERR_NOT_FOUND);
    expect("full: bit untouched", bit == 999U);
}

/* 70 bits: bits 70..127 of the storage are not part of the bitmap and must never be returned. */
static void test_find_zero_ignores_tail(void)
{
    uint64_t storage[2] = {UINT64_MAX, (1ULL << 6) - 1U};
    struct bitmap b = {storage, 70};
    size_t bit = 999;

    expect("tail bits ignored", bitmap_find_zero(&b, &bit) == KERR_NOT_FOUND);
}

int main(void)
{
    test_words();
    test_set_clear();
    test_find_zero();
    test_find_zero_ignores_tail();

    if (failures != 0) {
        printf("bitmap_test: %d failed\n", failures);
        return 1;
    }
    printf("PASS  bitmap_test\n");
    return 0;
}
