#ifndef ANVIL_CONSOLE_H
#define ANVIL_CONSOLE_H

#include <stdarg.h>

/** Writes one raw byte to the output device (no newline translation). */
typedef void (*console_putc_fn)(char c);

/**
 * Set where kprintf() output goes. Output is dropped until then.
 *
 * @param putc  Output function, e.g. pl011_putc. Must not be NULL.
 *
 * Context: early boot, single CPU, before any kprintf().
 */
void console_init(console_putc_fn putc);

/**
 * Formatted print to the console, with kformat() rules. "\n" is sent as "\r\n".
 *
 * @param fmt  Format string. Must not be NULL.
 *
 * Context: single CPU, no locking yet.
 */
void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * kprintf() with a va_list.
 *
 * @param fmt   Format string. Must not be NULL.
 * @param args  Arguments matching `fmt`.
 *
 * Context: same as kprintf().
 */
void vkprintf(const char *fmt, va_list args) __attribute__((format(printf, 1, 0)));

#endif
