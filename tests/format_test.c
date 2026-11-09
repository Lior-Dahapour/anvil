/* Host unit tests for kformat() (`make test`, ASan + UBSan). */
#include <anvil/format.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 256U

struct buffer {
    char data[BUFFER_SIZE];
    size_t used;
};

static int failures;

static void buffer_emit(char c, void *ctx)
{
    struct buffer *buf = ctx;
    if (buf->used + 1U < BUFFER_SIZE) {
        buf->data[buf->used] = c;
        buf->used++;
        buf->data[buf->used] = '\0';
    }
}

/* Format into a fresh buffer; text and count must match. */
static void check_va(const char *expected, const char *fmt, va_list args)
{
    struct buffer buf = {.data = {0}, .used = 0};

    size_t count = kformat(buffer_emit, &buf, fmt, args);

    if (strcmp(buf.data, expected) != 0 || count != strlen(expected)) {
        printf("FAIL  format \"%s\": expected \"%s\" (%zu), got \"%s\" (%zu)\n", fmt, expected,
               strlen(expected), buf.data, count);
        failures++;
    }
}

/* Compiler-checked, catches mistakes in the tests themselves. */
static void check(const char *expected, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void check(const char *expected, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    check_va(expected, fmt, args);
    va_end(args);
}

/* Not compiler-checked, for deliberately malformed formats. */
static void check_malformed(const char *expected, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    check_va(expected, fmt, args);
    va_end(args);
}

int main(void)
{
    /* Plain text and escapes. */
    check("", "");
    check("Anvil\n", "Anvil\n");
    check("100%", "100%%");

    /* Characters and strings. */
    check("A", "%c", 'A');
    check("hello world", "%s %s", "hello", "world");
    check("(null)", "%s", (const char *)NULL);

    /* Signed decimal, including the extremes. */
    check("0", "%d", 0);
    check("-42", "%d", -42);
    check("2147483647", "%i", INT_MAX);
    check("-2147483648", "%d", INT_MIN);
    check("-9223372036854775808", "%ld", LONG_MIN);
    check("9223372036854775807", "%lld", LLONG_MAX);

    /* Unsigned and hex. */
    check("4294967295", "%u", UINT_MAX);
    check("18446744073709551615", "%lu", ULONG_MAX);
    check("ff", "%x", 255U);
    check("FF", "%X", 255U);
    check("deadbeef", "%lx", 0xdeadbeefUL);
    check("ffffffffffffffff", "%llx", ULLONG_MAX);
    check("123", "%zu", (size_t)123);

    /* Width and padding. */
    check("   42", "%5d", 42);
    check("00042", "%05d", 42);
    check("  -42", "%5d", -42);
    check("-0042", "%05d", -42);
    check("12345", "%3d", 12345);
    check("0000000009000000", "%016lx", 0x09000000UL);

    /* '-' flag: align left, pad on the right. It wins over '0', as in printf. */
    check("42   |", "%-5d|", 42);
    check("-42  |", "%-5d|", -42);
    check("x9 |", "x%-2zu|", (size_t)9);
    check("x30|", "x%-2zu|", (size_t)30);
    check_malformed("ff   |", "%-05x|", 255U);
    check_malformed("ff   |", "%0-5x|", 255U);

    /* Width for strings and characters. */
    check("   ab|", "%5s|", "ab");
    check("ab   |", "%-5s|", "ab");
    check("toolong|", "%3s|", "toolong");
    check("  A|", "%3c|", 'A');
    check("A  |", "%-3c|", 'A');

    /* Arguments after a flagged spec are still read in order (no shift). */
    check("x1  00000000000000aa", "x%-2zu %016lx", (size_t)1, 0xaaUL);

    /* Pointers: always 0x + 16 digits. */
    check("0x0000000040200000", "%p", (void *)(uintptr_t)0x40200000U);
    check("0x0000000000000000", "%p", (void *)NULL);

    /* Unsupported or truncated specs are printed as written, without consuming arguments. */
    check_malformed("%q", "%q");
    check_malformed("50%", "50%");

    if (failures != 0) {
        printf("format_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("format_test: all passed\n");
    return 0;
}
