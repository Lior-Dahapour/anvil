#ifndef ANVIL_TYPES_H
#define ANVIL_TYPES_H

#include <stdint.h>

/* Distinct types, so physical and virtual addresses cannot be mixed up. */
typedef struct {
    uint64_t value;
} phys_addr_t;

typedef struct {
    uint64_t value;
} virt_addr_t;

#endif
