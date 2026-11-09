#include <anvil/console.h>
#include <anvil/format.h>
#include <anvil/panic.h>
#include <stdarg.h>
#include <stddef.h>

/* Set once by console_init() in early boot, read-only after. */
static console_putc_fn console_out;

void console_init(console_putc_fn putc)
{
    KASSERT(putc != NULL);
    console_out = putc;
}

/* Terminals need "\r\n" to go back to the first column. */
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

void vkprintf(const char *fmt, va_list args)
{
    kformat(console_emit, NULL, fmt, args);
}

void kprintf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vkprintf(fmt, args);
    va_end(args);
}
