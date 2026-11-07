#include <anvil/fdt.h>

/* Devicetree Specification v0.4, 5.2 (Header). */
#define FDT_MAGIC 0xd00dfeedU
#define FDT_HEADER_SIZE 40U

/* Device tree numbers are big-endian (most significant byte first), our CPU is little-endian.
 * Building the value byte by byte gives the right number on any CPU, and byte loads can never
 * be misaligned.
 * Safety: the caller guarantees 4 readable bytes at `bytes`. */
static uint32_t read_be32_unchecked(const uint8_t *bytes)
{
    /*
     * SAFETY: the caller guarantees `bytes` points at 4 readable bytes inside the device tree
     * (checked against the header's totalsize before any read).
     */
    return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 | (uint32_t)bytes[2] << 8 |
           (uint32_t)bytes[3];
}

kerr_t fdt_check_header(const uint8_t *blob, size_t len)
{
    if (len < FDT_HEADER_SIZE) {
        return KERR_INVALID;
    }

    /*
     * SAFETY: `len` >= 40 is checked above and the caller guarantees `len` bytes at `blob` are
     * readable, so the magic (offset 0) and totalsize (offset 4) fields are in bounds.
     */
    if (read_be32_unchecked(blob) != FDT_MAGIC) {
        return KERR_INVALID;
    }

    // SAFETY : above we know the len is enough , so there is no garbage value here
    const uint32_t totalsize = read_be32_unchecked(blob + 4);
    if (totalsize > len || totalsize < FDT_HEADER_SIZE) {
        return KERR_INVALID;
    }

    return KERR_OK;
}
