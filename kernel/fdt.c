#include <anvil/fdt.h>
#include <anvil/panic.h>
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

/* Device tree numbers are big-endian. Byte loads work on any CPU and are never misaligned.
 * Safety: the caller guarantees 4 readable bytes at `bytes`. */
static uint32_t read_be32_unchecked(const uint8_t *bytes)
{
    /* SAFETY: 4 readable bytes at `bytes`, guaranteed by the caller. */
    return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 | (uint32_t)bytes[2] << 8 |
           (uint32_t)bytes[3];
}

/* Bounds-checked read for offsets computed from the tree. After fdt_check_header totalsize >= 40,
 * so `totalsize - 4U` cannot wrap, unlike `offset + 4U` with a huge offset. */
static KMUST_CHECK kerr_t read_be32(const uint8_t *blob, uint32_t totalsize, uint32_t offset,
                                    uint32_t *value)
{
    if (offset > totalsize - 4U) {
        return KERR_INVALID;
    }

    /* SAFETY: offset + 4 <= totalsize (above); totalsize bytes readable (fdt_check_header). */
    *value = read_be32_unchecked(blob + offset);
    return KERR_OK;
}

kerr_t fdt_check_header(const uint8_t *blob, const size_t len)
{
    if (len < FDT_HEADER_SIZE) {
        return KERR_INVALID;
    }

    /*
     * SAFETY: len >= 40 (above) and the caller guarantees len readable bytes, so the header reads
     * below are in bounds. The values are untrusted until checked.
     */
    if (read_be32_unchecked(blob) != FDT_MAGIC) {
        return KERR_INVALID;
    }

    const uint32_t totalsize = read_be32_unchecked(blob + 4);
    if (totalsize > len || totalsize < FDT_HEADER_SIZE) {
        return KERR_INVALID;
    }

    /* The token list must start inside the tree. */
    const uint32_t struct_offset = read_be32_unchecked(blob + 8);
    if (struct_offset >= totalsize) {
        return KERR_INVALID;
    }

    /* The strings block may be empty but must fit. The `||` order keeps the subtraction from
     * wrapping. */
    const uint32_t strings_offset = read_be32_unchecked(blob + 12);
    const uint32_t strings_size = read_be32_unchecked(blob + 32);
    if (strings_offset > totalsize || strings_size > totalsize - strings_offset) {
        return KERR_INVALID;
    }

    return KERR_OK;
}

/* Bounded strcmp of the property name at `nameoff`. A name without a 0 byte does not match. */
static bool name_is(const uint8_t *blob, const uint32_t strings_offset, const uint32_t strings_size,
                    const uint32_t nameoff, const char *expected)
{
    for (uint32_t i = 0;; i++) {
        const uint32_t pos = nameoff + i;
        if (pos >= strings_size) {
            return false;
        }
        /* SAFETY: pos < strings_size (above); strings block inside tree (fdt_check_header). */
        const uint8_t c = blob[strings_offset + pos];
        if (c != (uint8_t)expected[i]) {
            return false;
        }
        if (c == 0) {
            return true;
        }
    }
}

/* "memory" or "memory@<unit-address>" (DT spec v0.4, 2.2.1 and 3.4).
 * Safety: the caller guarantees `name_len` readable bytes at `name`. */
static bool node_is_memory_unchecked(const uint8_t *name, uint32_t name_len)
{
    const char *memory = "memory";

    if (name_len < 6U) {
        return false;
    }
    /* SAFETY: name_len >= 6 (above); name[6] is read only when name_len > 6. */
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

/* A 1- or 2-cell number, high cell first (DT spec v0.4, 2.2.4). */
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
        /* offset <= totalsize - 4 (read above), so offset + 4U cannot wrap. */
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
KMUST_CHECK kerr_t fdt_total_size(const uint8_t *blob, const size_t len, uint32_t *size)
{
    KASSERT(size != NULL);
    if (fdt_check_header(blob, len) != KERR_OK) {
        return KERR_INVALID;
    }
    // SAFETY: header fields, in bounds after fdt_check_header.
    *size = read_be32_unchecked(blob + 4);
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
             * Name: 0-terminated, padded to 4 bytes (DT spec v0.4, 5.4.1). A corrupt tree may
             * have no 0.
             * SAFETY: offset < totalsize before every read; readable (fdt_check_header).
             */
            while (offset < totalsize && blob[offset] != 0) {
                offset++;
            }
            if (offset == totalsize) {
                return KERR_INVALID;
            }
            /* SAFETY: the name is blob[name_start..offset), inside the tree (0 found above). */
            if (depth == 2U && node_is_memory_unchecked(blob + name_start, offset - name_start)) {
                in_memory = true;
            }
            offset++;
            /* Room for the next token. Also keeps `offset + 3U` from wrapping. */
            if (offset > totalsize - 4U) {
                return KERR_INVALID;
            }
            offset = (offset + 3U) & ~3U;
            continue;
        }
        if (token == FDT_PROP) {
            /* PROP: length, name offset, value, padding to 4 bytes (DT spec v0.4, 5.4.1). */
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
            /* First (address, size) pair of /memory reg. Further ranges are not used yet. */
            if (in_memory && depth == 2U &&
                name_is(blob, strings_offset, strings_size, nameoff, "reg")) {
                /* Checked first, so the length below is at most 16 and cannot wrap. */
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
            /* prop_len is untrusted: compare with the bytes left, `offset + prop_len` could
             * wrap. */
            if (prop_len > totalsize - offset) {
                return KERR_INVALID;
            }
            offset += prop_len;

            /* As after a node name. */
            if (offset > totalsize - 4U) {
                return KERR_INVALID;
            }
            offset = (offset + 3U) & ~3U;
            continue;
        }
        return KERR_INVALID;
    }
}
