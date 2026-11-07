#include "boot.h"

/* Defined here, written by boot.S. Zero until boot.S stores the real value. */
uint64_t arch_boot_el;
/* Defined here, written by boot.S. Zero until boot.S stores the real value. */
uint64_t arch_boot_dtb;