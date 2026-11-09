#include "boot.h"

#include <arch/boot.h>

/* Written once by boot.S. */
uint64_t arch_boot_el;
uint64_t arch_boot_dtb;

phys_addr_t arch_boot_dtb_address(void)
{
    /* Saved with the MMU off, so it is physical. */
    phys_addr_t dtb = {.value = arch_boot_dtb};
    return dtb;
}
