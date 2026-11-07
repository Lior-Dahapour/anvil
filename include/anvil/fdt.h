#ifndef ANVIL_FDT_H
#define ANVIL_FDT_H

#include <anvil/kerr.h>
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
 *         magic number is wrong, or the tree's total size is smaller than a header or larger
 *         than `len`.
 *
 * Context: any.
 */
KMUST_CHECK kerr_t fdt_check_header(const uint8_t *blob, size_t len);

#endif
