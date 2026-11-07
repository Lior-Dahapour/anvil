#include "boot.h"

#include <arch/boot.h>

/* Defined here, written by boot.S. Zero until boot.S stores the real value. */
uint64_t arch_boot_el;
/* Defined here, written by boot.S. Zero until boot.S stores the real value. */
uint64_t arch_boot_dtb;

phys_addr_t arch_boot_dtb_address(void)
{
    /* boot.S stored x0 from the bootloader, with the MMU off, so it is a physical address. */
    phys_addr_t dtb = {.value = arch_boot_dtb};
    return dtb;
}
