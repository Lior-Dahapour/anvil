#include <anvil/fdt.h>
#include <stdbool.h>

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
     * readable, so every header field read below (offsets 0, 4, 8, 12, 32) is in bounds. The values
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

    /* The strings block (property names) must lie inside the tree; it may be empty. `||`
     * evaluates left to right, so `totalsize - strings_offset` only runs once it cannot wrap. */
    const uint32_t strings_offset = read_be32_unchecked(blob + 12);
    const uint32_t strings_size = read_be32_unchecked(blob + 32);
    if (strings_offset > totalsize || strings_size > totalsize - strings_offset) {
        return KERR_INVALID;
    }

    return KERR_OK;
}
/* Does the property name at `nameoff` in the strings block equal `expected`? A bounded strcmp:
 * a name that runs off the end of the strings block (no 0 byte) is simply not equal. The
 * strings block must have passed fdt_check_header. */
static bool name_is(const uint8_t *blob, const uint32_t strings_offset, const uint32_t strings_size,
                    const uint32_t nameoff, const char *expected)
{
    for (uint32_t i = 0;; i++) {
        const uint32_t pos = nameoff + i;
        if (pos >= strings_size) {
            return false;
        }
        /*
         * SAFETY: pos < strings_size (checked above) and the strings block fits inside the
         * tree (fdt_check_header), so strings_offset + pos < totalsize: in bounds, no wrap.
         */
        const uint8_t c = blob[strings_offset + pos];
        if (c != (uint8_t)expected[i]) {
            return false;
        }
        if (c == 0) {
            return true;
        }
    }
}
/* Is this the /memory node? Its name is "memory", optionally followed by "@" and a unit address
 * (DT spec v0.4, 2.2.1 and 3.4). The unit address is only a label: the real range is in `reg`,
 * so it is not parsed.
 * Safety: the caller guarantees `name_len` readable bytes at `name`. */
static bool node_is_memory_unchecked(const uint8_t *name, uint32_t name_len)
{
    const char *memory = "memory";

    if (name_len < 6U) {
        return false;
    }
    /* SAFETY: name_len >= 6 is checked above, so name[0..5] (and name[6] below, once
     * name_len > 6) are within the caller's name_len bytes. */
    for (uint32_t i = 0; i < 6U; i++) {
        if (name[i] != (uint8_t)memory[i]) {
            return false;
        }
    }
    if (name_len == 6U) {
        return true;
    }
    return name[6] == '@';
}
/* Read a 1- or 2-cell number (#address-cells / #size-cells) at `offset`. With 2 cells the first
 * is the high half (DT spec v0.4, 2.2.4: values are big-endian). Other cell counts are not
 * supported, as in Linux on 64-bit. */
static KMUST_CHECK kerr_t read_cells(const uint8_t *blob, uint32_t totalsize, uint32_t offset,
                                     uint32_t cells, uint64_t *value)
{
    if (cells == 1U) {
        uint32_t low = 0;
        if (read_be32(blob, totalsize, offset, &low) != KERR_OK) {
            return KERR_INVALID;
        }
        *value = low;
    } else if (cells == 2U) {
        uint32_t high = 0;
        if (read_be32(blob, totalsize, offset, &high) != KERR_OK) {
            return KERR_INVALID;
        }
        /* The read above proved offset <= totalsize - 4, so offset + 4U cannot wrap. */
        uint32_t low = 0;
        if (read_be32(blob, totalsize, offset + 4U, &low) != KERR_OK) {
            return KERR_INVALID;
        }
        *value = (uint64_t)high << 32 | low;
    } else {
        return KERR_INVALID;
    }
    return KERR_OK;
}
kerr_t fdt_find_memory(const uint8_t *blob, const size_t len, phys_addr_t *base, uint64_t *size)
{
    if (fdt_check_header(blob, len) != KERR_OK) {
        return KERR_INVALID;
    }

    /* SAFETY: header fields, in bounds after fdt_check_header. */
    const uint32_t totalsize = read_be32_unchecked(blob + 4);
    uint32_t offset = read_be32_unchecked(blob + 8);
    const uint32_t strings_offset = read_be32_unchecked(blob + 12);
    const uint32_t strings_size = read_be32_unchecked(blob + 32);
    uint32_t depth = 0;
    uint32_t address_cells = 2U;
    uint32_t size_cells = 1U;
    bool in_memory = false;
    for (;;) {
        uint32_t token = 0U;
        if (read_be32(blob, totalsize, offset, &token) != KERR_OK) {
            return KERR_INVALID;
        }
        offset += 4U;
        if (token == FDT_END) {
            if (depth != 0) {
                return KERR_INVALID;
            }
            return KERR_NOT_FOUND;
        }
        if (token == FDT_NOP) {
            continue;
        }
        if (token == FDT_END_NODE) {
            if (depth == 0) {
                return KERR_INVALID;
            }
            if (depth == 2U) {
                in_memory = false;
            }
            depth--;
            continue;
        }
        if (token == FDT_BEGIN_NODE) {
            depth++;
            const uint32_t name_start = offset;
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
            /*
             * SAFETY: the name is blob[name_start .. offset), and offset < totalsize (the 0
             * byte was found, see above), so all name_len bytes are inside the tree, which
             * fdt_check_header verified is readable. offset >= name_start, so the length
             * cannot wrap.
             */
            if (depth == 2U && node_is_memory_unchecked(blob + name_start, offset - name_start)) {
                in_memory = true;
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
            const uint32_t value_offset = offset;
            if (depth == 1U &&
                name_is(blob, strings_offset, strings_size, nameoff, "#address-cells")) {
                if (prop_len != 4U) {
                    return KERR_INVALID;
                }
                if (read_be32(blob, totalsize, value_offset, &address_cells) != KERR_OK) {
                    return KERR_INVALID;
                }
            }
            if (depth == 1U &&
                name_is(blob, strings_offset, strings_size, nameoff, "#size-cells")) {
                if (prop_len != 4U) {
                    return KERR_INVALID;
                }

                if (read_be32(blob, totalsize, value_offset, &size_cells) != KERR_OK) {
                    return KERR_INVALID;
                }
            }
            /* The memory node's reg: the first (address, size) pair is the RAM range. Other
             * pairs (RAM split into several ranges) are not used yet. */
            if (in_memory && depth == 2U &&
                name_is(blob, strings_offset, strings_size, nameoff, "reg")) {
                /* Only 1 or 2 cells are supported (as in read_cells). Checked first, so the
                 * length below is at most 16 bytes and cannot wrap. */
                if (address_cells < 1U || address_cells > 2U || size_cells < 1U ||
                    size_cells > 2U) {
                    return KERR_INVALID;
                }
                if (prop_len < (address_cells + size_cells) * 4U) {
                    return KERR_INVALID;
                }
                uint64_t ram_base = 0;
                uint64_t ram_size = 0;
                if (read_cells(blob, totalsize, value_offset, address_cells, &ram_base) !=
                    KERR_OK) {
                    return KERR_INVALID;
                }
                if (read_cells(blob, totalsize, value_offset + address_cells * 4U, size_cells,
                               &ram_size) != KERR_OK) {
                    return KERR_INVALID;
                }
                base->value = ram_base;
                *size = ram_size;
                return KERR_OK;
            }
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