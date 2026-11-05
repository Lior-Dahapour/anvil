#ifndef ANVIL_TYPES_H
#define ANVIL_TYPES_H

#include <stdint.h>

/* Separate struct types so the compiler rejects passing a physical address where a virtual
 * one is expected (and the other way round). */
typedef struct {
    uint64_t value;
} phys_addr_t;

typedef struct {
    uint64_t value;
} virt_addr_t;

#endif
