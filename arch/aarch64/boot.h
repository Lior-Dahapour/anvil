#ifndef ANVIL_ARCH_AARCH64_BOOT_H
#define ANVIL_ARCH_AARCH64_BOOT_H

#include <stdint.h>

/**
 * Exception level the CPU was in when QEMU started the kernel (1 or 2).
 *
 * Written once by boot.S after .bss is zeroed and before kmain() runs; read-only afterwards.
 *
 * Context: any.
 */
extern uint64_t arch_boot_el;

/**
 * Physical address of the device tree blob, passed by the bootloader in x0 (arm64 Image boot
 * protocol). Physical because boot.S runs with the MMU off. Not validated: the DTB parser must
 * check it before reading.
 *
 * Written once by boot.S after .bss is zeroed and before kmain() runs; read-only afterwards.
 *
 * Context: any.
 */
extern uint64_t arch_boot_dtb;

#endif
