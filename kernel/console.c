#include <anvil/console.h>
#include <anvil/format.h>
#include <stdarg.h>
#include <stddef.h>

/* Set once by console_init() during single-CPU early boot, read-only afterwards. */
static console_putc_fn console_out;

void console_init(console_putc_fn putc)
{
    console_out = putc;
}

/* Terminals need "\r\n" to start a new line at the left edge; callers just write "\n". */
static void console_emit(char c, void *ctx)
{
    (void)ctx;
    if (console_out == NULL) {
        return;
    }
    if (c == '\n') {
        console_out('\r');
    }
    console_out(c);
}

void kprintf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    kformat(console_emit, NULL, fmt, args);
    va_end(args);
}
