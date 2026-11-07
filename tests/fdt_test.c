/*
 * Host unit tests for the device tree parser. Built and run on the development machine by
 * `make test`, with AddressSanitizer and UndefinedBehaviorSanitizer.
 */
#include <anvil/fdt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC 0xd00dfeedU
#define HEADER_SIZE 40U
#define TREE_SIZE 64U

static int failures;

/* Store `value` big-endian at `offset`, the way a device tree stores every number. */
static void put_be32(uint8_t *bytes, size_t offset, uint32_t value)
{
    bytes[offset] = (uint8_t)(value >> 24);
    bytes[offset + 1U] = (uint8_t)(value >> 16);
    bytes[offset + 2U] = (uint8_t)(value >> 8);
    bytes[offset + 3U] = (uint8_t)value;
}

/*
 * Arrange: a TREE_SIZE-byte tree with the given header fields, copied into a heap buffer
 * of exactly `len` bytes, so AddressSanitizer reports any read past `len`.
 * Act: fdt_check_header. Assert: the expected result.
 */
static void check_tree(const char *name, uint32_t magic, uint32_t totalsize, uint32_t struct_offset,
                       size_t len, kerr_t expected)
{
    uint8_t tree[TREE_SIZE] = {0};
    put_be32(tree, 0, magic);
    put_be32(tree, 4, totalsize);
    put_be32(tree, 8, struct_offset);

    uint8_t *blob = malloc(len == 0 ? 1 : len);
    if (blob == NULL) {
        printf("FAIL  %s: out of memory\n", name);
        failures++;
        return;
    }
    memcpy(blob, tree, len < TREE_SIZE ? len : TREE_SIZE);

    kerr_t result = fdt_check_header(blob, len);
    free(blob);

    if (result != expected) {
        printf("FAIL  %s: expected %d, got %d\n", name, expected, result);
        failures++;
    }
}

/* Most cases only vary magic and totalsize: the token list starts right after the header. */
static void check(const char *name, uint32_t magic, uint32_t totalsize, size_t len, kerr_t expected)
{
    check_tree(name, magic, totalsize, HEADER_SIZE, len, expected);
}

/*
 * Arrange: a valid header followed by `count` tokens, nothing else. Act: fdt_find_memory.
 * Assert: the expected result. The buffer is exactly the tree's size, so AddressSanitizer
 * reports any read past the end.
 */
static void check_tokens(const char *name, const uint32_t *tokens, size_t count, kerr_t expected)
{
    size_t size = HEADER_SIZE + 4U * count;
    uint8_t *blob = calloc(size, 1);
    if (blob == NULL) {
        printf("FAIL  %s: out of memory\n", name);
        failures++;
        return;
    }
    put_be32(blob, 0, MAGIC);
    put_be32(blob, 4, (uint32_t)size);
    put_be32(blob, 8, HEADER_SIZE);
    for (size_t i = 0; i < count; i++) {
        put_be32(blob, HEADER_SIZE + 4U * i, tokens[i]);
    }

    phys_addr_t base = {.value = 0};
    uint64_t ram_size = 0;
    kerr_t result = fdt_find_memory(blob, size, &base, &ram_size);
    free(blob);

    if (result != expected) {
        printf("FAIL  %s: expected %d, got %d\n", name, expected, result);
        failures++;
    }
}

/* Arrange: a device tree file dumped by QEMU. Act: fdt_find_memory. Assert: the result. */
static void check_file(const char *path, kerr_t expected)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        printf("FAIL  %s: cannot open\n", path);
        failures++;
        return;
    }
    static uint8_t tree[1U << 20];
    size_t size = fread(tree, 1, sizeof(tree), file);
    fclose(file);

    /* Exactly `size` bytes on the heap, so AddressSanitizer catches reads past the end. */
    uint8_t *blob = malloc(size);
    if (blob == NULL) {
        printf("FAIL  %s: out of memory\n", path);
        failures++;
        return;
    }
    memcpy(blob, tree, size);

    phys_addr_t base = {.value = 0};
    uint64_t ram_size = 0;
    kerr_t result = fdt_find_memory(blob, size, &base, &ram_size);
    free(blob);

    if (result != expected) {
        printf("FAIL  %s: expected %d, got %d\n", path, expected, result);
        failures++;
    }
}

int main(int argc, char **argv)
{
    /* Valid headers: a tree that fills `len`, and the smallest one (one token after the header). */
    check("valid", MAGIC, TREE_SIZE, TREE_SIZE, KERR_OK);
    check("tree smaller than len", MAGIC, HEADER_SIZE + 4U, TREE_SIZE, KERR_OK);

    /* Only a header: the token list would start right after the end of the tree. */
    check("header-only tree", MAGIC, HEADER_SIZE, HEADER_SIZE, KERR_INVALID);

    /* Not even room for a header: must fail without reading anything. */
    check("len 0", MAGIC, TREE_SIZE, 0, KERR_INVALID);
    /* Ends inside the totalsize field: without the len check this reads 1 byte past the end,
     * which AddressSanitizer reports. */
    check("len 7", MAGIC, TREE_SIZE, 7, KERR_INVALID);
    check("len one short of a header", MAGIC, TREE_SIZE, HEADER_SIZE - 1U, KERR_INVALID);

    /* Wrong magic. The byte-swapped one is what a little-endian read of a real tree gives. */
    check("magic off by one", MAGIC + 1U, TREE_SIZE, TREE_SIZE, KERR_INVALID);
    check("magic byte-swapped", 0xedfe0dd0U, TREE_SIZE, TREE_SIZE, KERR_INVALID);
    check("magic zero", 0, TREE_SIZE, TREE_SIZE, KERR_INVALID);

    /* totalsize smaller than the header itself. */
    check("totalsize 0", MAGIC, 0, TREE_SIZE, KERR_INVALID);
    check("totalsize one short of a header", MAGIC, HEADER_SIZE - 1U, TREE_SIZE, KERR_INVALID);

    /* totalsize claims more bytes than the caller allows to be read. */
    check("totalsize one past len", MAGIC, TREE_SIZE + 1U, TREE_SIZE, KERR_INVALID);
    check("totalsize max", MAGIC, 0xffffffffU, TREE_SIZE, KERR_INVALID);

    /* Structure block offset: must point inside the tree. Equal to totalsize is one past it. */
    check_tree("struct offset last byte", MAGIC, TREE_SIZE, TREE_SIZE - 1U, TREE_SIZE, KERR_OK);
    check_tree("struct offset at totalsize", MAGIC, TREE_SIZE, TREE_SIZE, TREE_SIZE, KERR_INVALID);
    check_tree("struct offset max", MAGIC, TREE_SIZE, 0xffffffffU, TREE_SIZE, KERR_INVALID);

    /* Walking the token list. No memory node anywhere, so a valid walk ends in NOT_FOUND. */
    check_tokens("only END", (const uint32_t[]){9}, 1, KERR_NOT_FOUND);
    check_tokens("NOPs then END", (const uint32_t[]){4, 4, 9}, 3, KERR_NOT_FOUND);
    check_tokens("END_NODE then END", (const uint32_t[]){2, 9}, 2, KERR_NOT_FOUND);
    check_tokens("unknown token 5", (const uint32_t[]){5, 9}, 2, KERR_INVALID);
    /* No END: the walk must stop at the end of the tree instead of reading past it. */
    check_tokens("tree ends without END", (const uint32_t[]){4, 4}, 2, KERR_INVALID);
    check_tokens("broken header", (const uint32_t[]){0}, 0, KERR_INVALID);

    /* Node names, written as big-endian words: "cpus" is 0x63707573. A name ends at its 0 byte
     * and is padded with zeros up to the next multiple of 4. */
    check_tokens("root node, empty name", (const uint32_t[]){1, 0, 2, 9}, 4, KERR_NOT_FOUND);
    check_tokens("name 3 letters, no padding", (const uint32_t[]){1, 0x61626300, 2, 9}, 4,
                 KERR_NOT_FOUND);
    check_tokens("name 4 letters, 3 padding", (const uint32_t[]){1, 0x63707573, 0, 2, 9}, 5,
                 KERR_NOT_FOUND);
    check_tokens("nested nodes", (const uint32_t[]){1, 0, 1, 0x63707573, 0, 2, 2, 9}, 8,
                 KERR_NOT_FOUND);
    /* Corrupt names: no 0 before the tree ends, or no room for a token after the name. */
    check_tokens("name without 0", (const uint32_t[]){1, 0x61626364}, 2, KERR_INVALID);
    check_tokens("name at the very end", (const uint32_t[]){1, 0x61626300}, 2, KERR_INVALID);

    /* Properties: PROP, len, nameoff, value, padding. */
    check_tokens("empty property", (const uint32_t[]){1, 0, 3, 0, 0, 2, 9}, 7, KERR_NOT_FOUND);
    check_tokens("4-byte value", (const uint32_t[]){1, 0, 3, 4, 0, 0x12345678, 2, 9}, 8,
                 KERR_NOT_FOUND);
    check_tokens("5-byte value, 3 padding",
                 (const uint32_t[]){1, 0, 3, 5, 0, 0x61626364, 0x65000000, 2, 9}, 9,
                 KERR_NOT_FOUND);
    /* Corrupt lengths: one byte too long, and a huge one that would wrap `offset + len`. */
    check_tokens("value 1 byte past the end", (const uint32_t[]){1, 0, 3, 5, 0, 0, 9}, 7,
                 KERR_INVALID);
    check_tokens("huge value length", (const uint32_t[]){1, 0, 3, 0xfffffffcU, 0, 2, 9}, 7,
                 KERR_INVALID);
    /* Wraps `offset` back exactly onto the PROP token: without the check the walk loops forever. */
    check_tokens("length wrapping back to PROP", (const uint32_t[]){1, 0, 3, 0xfffffff4U, 0, 2, 9},
                 7, KERR_INVALID);
    check_tokens("PROP cut off after len", (const uint32_t[]){1, 0, 3, 4}, 4, KERR_INVALID);

    /* QEMU's real device tree, dumped by `make test`. The walk must reach END; finding the
     * memory node comes next. */
    if (argc > 1) {
        check_file(argv[1], KERR_NOT_FOUND);
    }

    if (failures != 0) {
        printf("fdt_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("fdt_test: all passed\n");
    return 0;
}
