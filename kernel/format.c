#include <anvil/format.h>
#include <stdbool.h>
#include <stdint.h>

/* Widths beyond this are clamped: nothing in the kernel needs more, and it bounds the loop. */
#define MAX_WIDTH 64U

/* 2^64 - 1 has 20 decimal digits, the longest number we can print. */
#define MAX_DIGITS 20U

struct output {
    kformat_emit_fn emit;
    void *ctx;
    size_t count;
};

enum length {
    LENGTH_INT,
    LENGTH_LONG,
    LENGTH_LONG_LONG,
    LENGTH_SIZE,
};

static void put_char(struct output *out, char c)
{
    out->emit(c, out->ctx);
    out->count++;
}

static void put_repeated(struct output *out, char c, size_t times)
{
    for (size_t i = 0; i < times; i++) {
        put_char(out, c);
    }
}

static void put_string(struct output *out, const char *str)
{
    for (const char *p = str; *p != '\0'; p++) {
        put_char(out, *p);
    }
}

/*
 * Print `magnitude` in `base`, with a '-' in front when `negative`. Zero padding goes between
 * the sign and the digits ("-0042"), space padding goes before the sign ("  -42").
 */
static void put_number(struct output *out, uint64_t magnitude, bool negative, unsigned int base,
                       bool upper, size_t width, char pad)
{
    const char *digit_chars = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char digits[MAX_DIGITS];
    size_t count = 0;

    /* Digits come out lowest first, so they are stored reversed and printed backwards. */
    do {
        digits[count] = digit_chars[magnitude % base];
        count++;
        magnitude /= base;
    } while (magnitude != 0U);

    size_t length = count + (negative ? 1U : 0U);
    size_t padding = width > length ? width - length : 0U;

    if (pad == ' ') {
        put_repeated(out, ' ', padding);
    }
    if (negative) {
        put_char(out, '-');
    }
    if (pad == '0') {
        put_repeated(out, '0', padding);
    }
    while (count > 0U) {
        count--;
        put_char(out, digits[count]);
    }
}

static int64_t read_signed(va_list *args, enum length length)
{
    switch (length) {
    case LENGTH_INT:
        return va_arg(*args, int);
    case LENGTH_LONG:
        return va_arg(*args, long);
    case LENGTH_LONG_LONG:
        return va_arg(*args, long long);
    case LENGTH_SIZE:
        return va_arg(*args, ptrdiff_t);
    }
    return 0;
}

static uint64_t read_unsigned(va_list *args, enum length length)
{
    switch (length) {
    case LENGTH_INT:
        return va_arg(*args, unsigned int);
    case LENGTH_LONG:
        return va_arg(*args, unsigned long);
    case LENGTH_LONG_LONG:
        return va_arg(*args, unsigned long long);
    case LENGTH_SIZE:
        return va_arg(*args, size_t);
    }
    return 0;
}

size_t kformat(kformat_emit_fn emit, void *ctx, const char *fmt, va_list args)
{
    struct output out = {.emit = emit, .ctx = ctx, .count = 0};
    va_list ap;

    /* Work on a copy so the helpers can take it by pointer (a va_list parameter may be an
     * array type, so `&args` would not have the type `va_list *`). */
    va_copy(ap, args);

    const char *p = fmt;
    while (*p != '\0') {
        if (*p != '%') {
            put_char(&out, *p);
            p++;
            continue;
        }

        const char *spec_start = p;
        p++;

        char pad = ' ';
        if (*p == '0') {
            pad = '0';
            p++;
        }

        size_t width = 0;
        while (*p >= '0' && *p <= '9') {
            width = width * 10U + (size_t)(*p - '0');
            if (width > MAX_WIDTH) {
                width = MAX_WIDTH;
            }
            p++;
        }

        enum length length = LENGTH_INT;
        if (*p == 'l') {
            length = LENGTH_LONG;
            p++;
            if (*p == 'l') {
                length = LENGTH_LONG_LONG;
                p++;
            }
        } else if (*p == 'z') {
            length = LENGTH_SIZE;
            p++;
        }

        char conversion = *p;
        switch (conversion) {
        case 'd':
        case 'i': {
            int64_t value = read_signed(&ap, length);
            /* Negate in unsigned arithmetic so INT64_MIN does not overflow. */
            uint64_t magnitude = value < 0 ? 0U - (uint64_t)value : (uint64_t)value;
            put_number(&out, magnitude, value < 0, 10U, false, width, pad);
            break;
        }
        case 'u':
            put_number(&out, read_unsigned(&ap, length), false, 10U, false, width, pad);
            break;
        case 'x':
        case 'X':
            put_number(&out, read_unsigned(&ap, length), false, 16U, conversion == 'X', width, pad);
            break;
        case 'p':
            put_string(&out, "0x");
            put_number(&out, (uint64_t)(uintptr_t)va_arg(ap, void *), false, 16U, false, 16U, '0');
            break;
        case 'c':
            put_char(&out, (char)va_arg(ap, int));
            break;
        case 's': {
            const char *str = va_arg(ap, const char *);
            put_string(&out, str != NULL ? str : "(null)");
            break;
        }
        case '%':
            put_char(&out, '%');
            break;
        default:
            /* Unsupported or truncated ("%" at the end): print the spec as written. */
            for (const char *q = spec_start; q < p; q++) {
                put_char(&out, *q);
            }
            continue;
        }
        p++;
    }

    va_end(ap);
    return out.count;
}
