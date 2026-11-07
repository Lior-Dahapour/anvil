#include <anvil/fdt.h>

/* Devicetree Specification v0.4, 5.2 (Header). */
#define FDT_MAGIC 0xd00dfeedU
#define FDT_HEADER_SIZE 40U

/* Devicetree Specification v0.4, 5.4.1 (Lexical structure). */
#define FDT_BEGIN_NODE 1U
#define FDT_END_NODE 2U
#define FDT_PROP 3U
#define FDT_NOP 4U
#define FDT_END 9U

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

/* Bounds-checked read of the 4 bytes at `offset`: the only way to read past the header, where
 * offsets are computed from untrusted lengths in the tree. `blob` and `totalsize` must have
 * passed fdt_check_header, so totalsize >= 40 and `totalsize - 4U` cannot wrap. Comparing
 * against `totalsize - 4U` instead of computing `offset + 4U` keeps a huge offset from wrapping
 * around to a small one. */
static KMUST_CHECK kerr_t read_be32(const uint8_t *blob, uint32_t totalsize, uint32_t offset,
                                    uint32_t *value)
{
    if (offset > totalsize - 4U) {
        return KERR_INVALID;
    }

    /*
     * SAFETY: offset + 4 <= totalsize is checked above, and fdt_check_header verified that
     * totalsize bytes at `blob` are readable, so all 4 bytes are inside the tree.
     */
    *value = read_be32_unchecked(blob + offset);
    return KERR_OK;
}

kerr_t fdt_check_header(const uint8_t *blob, const size_t len)
{
    if (len < FDT_HEADER_SIZE) {
        return KERR_INVALID;
    }

    /*
     * SAFETY: `len` >= 40 is checked above and the caller guarantees `len` bytes at `blob` are
     * readable, so every header field read below (offsets 0, 4 and 8) is in bounds. The values
     * themselves are untrusted until checked.
     */
    if (read_be32_unchecked(blob) != FDT_MAGIC) {
        return KERR_INVALID;
    }

    const uint32_t totalsize = read_be32_unchecked(blob + 4);
    if (totalsize > len || totalsize < FDT_HEADER_SIZE) {
        return KERR_INVALID;
    }

    /* Where the token list (structure block) starts. Equal to totalsize is already outside. */
    const uint32_t struct_offset = read_be32_unchecked(blob + 8);
    if (struct_offset >= totalsize) {
        return KERR_INVALID;
    }

    return KERR_OK;
}
kerr_t fdt_find_memory(const uint8_t *blob, const size_t len, phys_addr_t *base, uint64_t *size)
{
    (void)base;
    (void)size;
    if (fdt_check_header(blob, len) != KERR_OK) {
        return KERR_INVALID;
    }

    /* SAFETY: header fields, in bounds after fdt_check_header. */
    const uint32_t totalsize = read_be32_unchecked(blob + 4);
    uint32_t offset = read_be32_unchecked(blob + 8);
    for (;;) {
        uint32_t token = 0U;
        if (read_be32(blob, totalsize, offset, &token) != KERR_OK) {
            return KERR_INVALID;
        }
        offset += 4U;
        if (token == FDT_END) {
            return KERR_NOT_FOUND;
        }
        if (token == FDT_NOP || token == FDT_END_NODE) {
            continue;
        }
        if (token == FDT_BEGIN_NODE) {
            /*
             * The node name: a 0-terminated string with no stored length, then zero padding to
             * the next 4-byte boundary (DT spec v0.4, 5.4.1). A corrupt tree may have no 0.
             *
             * SAFETY: `offset < totalsize` is tested before every `blob[offset]` read, and
             * fdt_check_header verified that totalsize bytes at `blob` are readable.
             */
            while (offset < totalsize && blob[offset] != 0) {
                offset++;
            }
            if (offset == totalsize) {
                return KERR_INVALID;
            }

            offset++;

            /* A token must follow the name. Checking for room first also keeps `offset + 3U`
             * from wrapping in the round-up to a multiple of 4. */
            if (offset > totalsize - 4U) {
                return KERR_INVALID;
            }
            offset = (offset + 3U) & ~3U;
            continue;
        }
        if (token == FDT_PROP) {
            /* PROP is followed by the value's length, the name's offset in the strings block,
             * the value, and zero padding to a 4-byte boundary (DT spec v0.4, 5.4.1). */
            uint32_t prop_len = 0;
            if (read_be32(blob, totalsize, offset, &prop_len) != KERR_OK) {
                return KERR_INVALID;
            }
            offset += 4U;
            uint32_t nameoff = 0;
            if (read_be32(blob, totalsize, offset, &nameoff) != KERR_OK) {
                return KERR_INVALID;
            }
            offset += 4U;

            /* prop_len comes from the tree and can be anything: compare it with the bytes
             * left instead of computing `offset + prop_len`, which could wrap. The read above
             * ended at `offset`, so `totalsize - offset` cannot wrap. */
            if (prop_len > totalsize - offset) {
                return KERR_INVALID;
            }
            offset += prop_len;

            /* Same as after a node name: room for the next token, then round up. */
            if (offset > totalsize - 4U) {
                return KERR_INVALID;
            }
            offset = (offset + 3U) & ~3U;
            continue;
        }
        return KERR_INVALID;
    }
}