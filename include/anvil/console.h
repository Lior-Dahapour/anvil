#ifndef ANVIL_CONSOLE_H
#define ANVIL_CONSOLE_H

#include <stdarg.h>

/**
 * Sends one raw byte to the output device (no newline translation).
 *
 * @param c  The byte.
 */
typedef void (*console_putc_fn)(char c);

/**
 * Choose where kprintf() output goes. Until this is called, output is dropped.
 *
 * @param putc  Output function, for example pl011_putc. Must not be NULL.
 *
 * Context: early boot, single CPU. Call once, before any kprintf().
 */
void console_init(console_putc_fn putc);

/**
 * Formatted print to the console. Same format rules as kformat(). "\n" is sent as "\r\n".
 *
 * @param fmt  Format string. Must not be NULL.
 *
 * Context: single CPU, no locking yet. Not safe to call from two CPUs at once.
 */
void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * kprintf() for callers that already hold a va_list, such as kpanic().
 *
 * @param fmt   Format string. Must not be NULL.
 * @param args  Arguments matching `fmt`.
 *
 * Context: same as kprintf().
 */
void vkprintf(const char *fmt, va_list args) __attribute__((format(printf, 1, 0)));

#endif
