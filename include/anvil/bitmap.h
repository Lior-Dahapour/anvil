//
// Created by Lior Dahapour on 10/11/2026.
//
#ifndef ANVIL_BITMAP_H
#define ANVIL_BITMAP_H
#include <anvil/kerr.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Bit n is bit n % 64 of word[n / 64]. */
struct bitmap {
    /* Owned by the caller, at least bitmap_words(nbits) long. */
    uint64_t *word;
    /* Number of valid bits. */
    size_t nbits;
};

/**
 * Words needed to hold `nbits` bits, rounded up.
 *
 * Context: any.
 */
size_t bitmap_words(size_t nbits);

/**
 * True if `bit` is 1. `b` must not be NULL, `bit` < `b->nbits` (asserted).
 *
 * Context: any.
 */
bool bitmap_test(const struct bitmap *b, size_t bit);

/**
 * Set `bit` to 1. `b` must not be NULL, `bit` < `b->nbits` (asserted).
 *
 * Context: any. Not atomic: the caller serializes access.
 */
void bitmap_set(struct bitmap *b, size_t bit);

/**
 * Set `bit` to 0. `b` must not be NULL, `bit` < `b->nbits` (asserted).
 *
 * Context: any. Not atomic: the caller serializes access.
 */
void bitmap_clear(struct bitmap *b, size_t bit);

/**
 * Find the lowest 0 bit. Does not set it.
 *
 * @param bit  Out: its number. Untouched if none. Must not be NULL.
 *
 * @return KERR_OK, or KERR_NOT_FOUND if every bit is 1.
 *
 * Context: any.
 */
KMUST_CHECK kerr_t bitmap_find_zero(const struct bitmap *b, size_t *bit);

#endif // ANVIL_BITMAP_H
