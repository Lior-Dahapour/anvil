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
 * Arrange: a TREE_SIZE-byte tree with the given magic and totalsize, copied into a heap buffer
 * of exactly `len` bytes, so AddressSanitizer reports any read past `len`.
 * Act: fdt_check_header. Assert: the expected result.
 */
static void check(const char *name, uint32_t magic, uint32_t totalsize, size_t len, kerr_t expected)
{
    uint8_t tree[TREE_SIZE] = {0};
    put_be32(tree, 0, magic);
    put_be32(tree, 4, totalsize);

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

int main(void)
{
    /* Valid headers, including the smallest possible tree and a tree that fills `len`. */
    check("valid", MAGIC, TREE_SIZE, TREE_SIZE, KERR_OK);
    check("tree smaller than len", MAGIC, HEADER_SIZE, TREE_SIZE, KERR_OK);
    check("header-only tree", MAGIC, HEADER_SIZE, HEADER_SIZE, KERR_OK);

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

    if (failures != 0) {
        printf("fdt_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("fdt_test: all passed\n");
    return 0;
}
