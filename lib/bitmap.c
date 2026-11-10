//
// Created by Lior Dahapour on 10/11/2026.
//
#include "anvil/panic.h"
#include <anvil/bitmap.h>
#include <anvil/kerr.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define WORD_BITS (sizeof(uint64_t) * 8)
size_t bitmap_words(const size_t nbits)
{
    // round up, because for example 65 bits need 2 not 1
    return (nbits + WORD_BITS - 1) / WORD_BITS;
}
bool bitmap_test(const struct bitmap *b, const size_t bit)
{
    KASSERT(b != NULL);
    KASSERT(bit < b->nbits);
    // SAFETY: caller guarantees that b hold bitmap_words(b->nbits) words
    // SAFETY: assert ensure we are in range and not null
    return (b->word[bit / WORD_BITS] & (1ULL << bit % WORD_BITS)) != 0;
}
void bitmap_set(struct bitmap *b, const size_t bit)
{
    KASSERT(b != NULL);
    KASSERT(bit < b->nbits);
    // SAFETY: caller guarantees that b hold bitmap_words(b->nbits) words
    // SAFETY: assert ensure we are in range and not null
    b->word[bit / WORD_BITS] |= (1ULL << bit % WORD_BITS);
}
void bitmap_clear(struct bitmap *b, const size_t bit)
{
    KASSERT(b != NULL);
    KASSERT(bit < b->nbits);
    // SAFETY: caller guarantees that b hold bitmap_words(b->nbits) words
    // SAFETY: assert ensure we are in range and not null
    b->word[bit / WORD_BITS] &= ~(1ULL << bit % WORD_BITS);
}
KMUST_CHECK kerr_t bitmap_find_zero(const struct bitmap *b, size_t *bit)
{
    KASSERT(b != NULL);
    KASSERT(bit != NULL);
    // SAFETY: assert ensure b isn't null
    for (size_t i = 0; i < bitmap_words(b->nbits); ++i) {
        // SAFETY: caller guarantees that b hold bitmap_words(b->nbits) words
        if (b->word[i] == UINT64_MAX) {
            // occupied
            continue;
        }
        // SAFETY: word is not all ones (checked above), so ~word != 0 as ctzll requires.
        const size_t bit_pos = (size_t)__builtin_ctzll(~b->word[i]) + WORD_BITS * i;
        if (bit_pos >= b->nbits) {
            return KERR_NOT_FOUND;
        }
        *bit = bit_pos;
        return KERR_OK;
    }
    return KERR_NOT_FOUND;
}