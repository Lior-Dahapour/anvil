#include <anvil/format.h>
#include <stdbool.h>
#include <stdint.h>

/* Larger widths are clamped, which bounds the padding loop. */
#define MAX_WIDTH 64U

/* UINT64_MAX has 20 decimal digits. */
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

/* Layout of one conversion. */
struct spec {
    size_t width;      /* minimum width */
    char pad;          /* ' ' or '0', ignored when `left` */
    bool left;         /* '-' flag */
    unsigned int base; /* 10 or 16 */
    bool upper;        /* %X */
};

static void put_string(struct output *out, const char *str)
{
    for (const char *p = str; *p != '\0'; p++) {
        put_char(out, *p);
    }
}

static size_t string_length(const char *str)
{
    size_t length = 0;
    while (str[length] != '\0') {
        length++;
    }
    return length;
}

/* %s and %c, padded with spaces. */
static void put_padded_string(struct output *out, const struct spec *spec, const char *str)
{
    size_t length = string_length(str);
    size_t padding = spec->width > length ? spec->width - length : 0U;

    if (!spec->left) {
        put_repeated(out, ' ', padding);
    }
    put_string(out, str);
    if (spec->left) {
        put_repeated(out, ' ', padding);
    }
}

/* Zero padding goes after the sign ("-0042"), space padding before it ("  -42"). */
static void put_number(struct output *out, const struct spec *spec, uint64_t magnitude,
                       bool negative)
{
    const char *digit_chars = spec->upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char digits[MAX_DIGITS];
    size_t count = 0;

    /* Lowest digit first, printed in reverse below. */
    do {
        digits[count] = digit_chars[magnitude % spec->base];
        count++;
        magnitude /= spec->base;
    } while (magnitude != 0U);

    size_t length = count + (negative ? 1U : 0U);
    size_t padding = spec->width > length ? spec->width - length : 0U;
    bool zero_pad = spec->pad == '0' && !spec->left;

    if (!spec->left && !zero_pad) {
        put_repeated(out, ' ', padding);
    }
    if (negative) {
        put_char(out, '-');
    }
    if (zero_pad) {
        put_repeated(out, '0', padding);
    }
    while (count > 0U) {
        count--;
        put_char(out, digits[count]);
    }
    if (spec->left) {
        put_repeated(out, ' ', padding);
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

    /* A va_list parameter may be an array type, so `&args` is not a `va_list *`. Use a copy. */
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

        struct spec spec = {.width = 0, .pad = ' ', .left = false, .base = 10U, .upper = false};

        while (*p == '-' || *p == '0') {
            if (*p == '-') {
                spec.left = true;
            } else {
                spec.pad = '0';
            }
            p++;
        }

        while (*p >= '0' && *p <= '9') {
            spec.width = spec.width * 10U + (size_t)(*p - '0');
            if (spec.width > MAX_WIDTH) {
                spec.width = MAX_WIDTH;
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
            /* Unsigned negation, so INT64_MIN does not overflow. */
            uint64_t magnitude = value < 0 ? 0U - (uint64_t)value : (uint64_t)value;
            put_number(&out, &spec, magnitude, value < 0);
            break;
        }
        case 'u':
            put_number(&out, &spec, read_unsigned(&ap, length), false);
            break;
        case 'x':
        case 'X':
            spec.base = 16U;
            spec.upper = conversion == 'X';
            put_number(&out, &spec, read_unsigned(&ap, length), false);
            break;
        case 'p': {
            /* Always "0x" and 16 digits, whatever the flags. */
            struct spec pointer = {.width = 16U, .pad = '0', .left = false, .base = 16U};
            put_string(&out, "0x");
            put_number(&out, &pointer, (uint64_t)(uintptr_t)va_arg(ap, void *), false);
            break;
        }
        case 'c': {
            char text[2] = {(char)va_arg(ap, int), '\0'};
            put_padded_string(&out, &spec, text);
            break;
        }
        case 's': {
            const char *str = va_arg(ap, const char *);
            put_padded_string(&out, &spec, str != NULL ? str : "(null)");
            break;
        }
        case '%':
            put_char(&out, '%');
            break;
        default:
            /* Unsupported or truncated: print as written. */
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
