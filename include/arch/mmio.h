#ifndef ANVIL_ARCH_MMIO_H
#define ANVIL_ARCH_MMIO_H

#include <anvil/types.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Read a 32-bit device register.
 *
 * @param base    Virtual address of the device's register block. Must be mapped as Device
 *                memory (or the MMU must be off).
 * @param offset  Byte offset of the register inside the block. Must be 4-byte aligned.
 *
 * @return The register value.
 *
 * Context: any.
 */
uint32_t mmio_read32(virt_addr_t base, size_t offset);

/**
 * Write a 32-bit device register.
 *
 * @param base    Same requirements as mmio_read32().
 * @param offset  Same requirements as mmio_read32().
 * @param value   Value to store.
 *
 * Context: any.
 */
void mmio_write32(virt_addr_t base, size_t offset, uint32_t value);

#endif
