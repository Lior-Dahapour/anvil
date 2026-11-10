//
// Created by Lior Dahapour on 10/11/2026.
//
#include <anvil/kerr.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <anvil/bitmap.h>
#include "anvil/panic.h"
#define  WORLD_BITS (sizeof(uint64_t) * 8 )
size_t bitmap_words(size_t nbits) {
    // round up, because for example 65 bits need 2 not 1
    return (nbits+ WORLD_BITS -1) / 64;
}
bool  bitmap_test(const struct bitmap *b, const size_t bit) {
    KASSERT(b != NULL);
    KASSERT(bit < b->nbits);
    // SAFETY : caller guarantees that b hold bitmap_words(b->nbits) words
    // SAFETY : assert ensure we are in range and not null
   return (b->word[bit/64] & (1ULL << bit % WORLD_BITS)) != 0;
}
void  bitmap_set(struct bitmap *b, const size_t bit) {
    KASSERT(b != NULL);
    KASSERT(bit < b->nbits);
    // SAFETY : caller guarantees that b hold bitmap_words(b->nbits) words
    // SAFETY : assert ensure we are in range and not null
    b->word[bit/64] |= (1ULL << bit % WORLD_BITS);
}
void  bitmap_clear(struct bitmap *b, const size_t bit) {
    KASSERT(b != NULL);
    KASSERT(bit < b->nbits);
    // SAFETY : caller guarantees that b hold bitmap_words(b->nbits) words
    // SAFETY : assert ensure we are in range and not null
    b->word[bit/64] &= ~(1ULL << bit % WORLD_BITS);
}
KMUST_CHECK kerr_t bitmap_find_zero(const struct bitmap *b, size_t *bit) {
    KASSERT(b != NULL);
    KASSERT(bit != NULL);
    size_t search = 0;
    // SAFETY : caller guarantees that b hold bitmap_words(b->nbits) words
    // SAFETY : assert ensure b isn't null
    while (search < b->nbits) {
         if (!bitmap_test(b,search)) {
             *bit = search;
             return KERR_OK;
         }
        search++;
    }
    return KERR_NOT_FOUND;
}