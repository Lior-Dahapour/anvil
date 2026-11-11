#ifndef ANVIL_FDT_H
#define ANVIL_FDT_H

#include <anvil/kerr.h>
#include <anvil/types.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Validate a flattened device tree header before anything else in the tree is read.
 *
 * @param blob  Start of the tree. Must not be NULL. Not modified.
 * @param len   Bytes the caller can safely read at `blob`.
 *
 * @return KERR_OK, or KERR_INVALID if the magic is wrong, totalsize is below a header or above
 *         `len`, or the structure or strings block is outside the tree.
 *
 * Context: any.
 */
KMUST_CHECK kerr_t fdt_check_header(const uint8_t *blob, size_t len);

/**
 * Size of the whole tree in bytes (the header's totalsize), for reserving its memory.
 *
 * @param blob  Start of the tree. Must not be NULL. Not modified.
 * @param len   Bytes the caller can safely read at `blob`.
 * @param size  Out: totalsize, at most `len`. Must not be NULL. Written only on success.
 *
 * @return KERR_OK, or KERR_INVALID if fdt_check_header rejects the tree.
 *
 * Context: any.
 */
KMUST_CHECK kerr_t fdt_total_size(const uint8_t *blob, size_t len, uint32_t *size);

/**
 * Find RAM: the first range of the /memory node's `reg`.
 *
 * @param blob  Start of the tree. Must not be NULL. Not modified.
 * @param len   Bytes the caller can safely read at `blob`.
 * @param base  Out: physical start of RAM. Must not be NULL. Written only on success.
 * @param size  Out: RAM size in bytes. Must not be NULL. Written only on success.
 *
 * @return KERR_OK, KERR_INVALID if the tree is malformed or the root's #address-cells or
 *         #size-cells is not 1 or 2, KERR_NOT_FOUND if there is no /memory `reg`.
 *
 * Context: any.
 */
KMUST_CHECK kerr_t fdt_find_memory(const uint8_t *blob, size_t len, phys_addr_t *base,
                                   uint64_t *size);

#endif
