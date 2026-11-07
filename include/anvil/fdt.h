#ifndef ANVIL_FDT_H
#define ANVIL_FDT_H

#include <anvil/kerr.h>
#include <anvil/types.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Check that `blob` starts with a valid flattened device tree header, before anything else in
 * the tree is read.
 *
 * @param blob  Start of the device tree. Must not be NULL. Not modified.
 * @param len   Number of bytes the caller can safely read at `blob`.
 *
 * @return KERR_OK if the header is valid, KERR_INVALID if `len` is too small for a header, the
 *         magic number is wrong, the tree's total size is smaller than a header or larger
 *         than `len`, the structure block offset is not inside the tree, or the strings
 *         block does not fit inside the tree.
 *
 * Context: any.
 */
KMUST_CHECK kerr_t fdt_check_header(const uint8_t *blob, size_t len);

/**
 * Find the machine's RAM in the device tree: the first range of the /memory node's `reg`.
 *
 * @param blob  Start of the device tree. Must not be NULL. Not modified.
 * @param len   Number of bytes the caller can safely read at `blob`.
 * @param base  Out: physical start address of RAM. Must not be NULL. Written only on success.
 * @param size  Out: size of RAM in bytes. Must not be NULL. Written only on success.
 *
 * @return KERR_OK on success, KERR_INVALID if the header or the token list is malformed or
 *         the root's #address-cells / #size-cells are not 1 or 2, KERR_NOT_FOUND if the tree
 *         is valid but has no /memory node with a reg property.
 *
 * Context: any.
 */
KMUST_CHECK kerr_t fdt_find_memory(const uint8_t *blob, size_t len, phys_addr_t *base,
                                   uint64_t *size);

#endif
