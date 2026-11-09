#ifndef ANVIL_FORMAT_H
#define ANVIL_FORMAT_H

#include <stdarg.h>
#include <stddef.h>

/** Receives each output character of kformat(), with the `ctx` passed to it. */
typedef void (*kformat_emit_fn)(char c, void *ctx);

/**
 * printf-style formatting that hands each character to `emit`.
 *
 * Supports %c %s %d %i %u %x %X %p %%, the l, ll and z modifiers, a width, and the '-' and '0'
 * flags. %p is "0x" plus 16 hex digits. A NULL %s prints "(null)". Anything else is printed as
 * written. No global state, so it also runs on the host for `make test`.
 *
 * @param emit  Character sink. Must not be NULL.
 * @param ctx   Passed to `emit`. May be NULL.
 * @param fmt   Format string. Must not be NULL.
 * @param args  Arguments matching `fmt`.
 *
 * @return Number of characters passed to `emit`.
 *
 * Context: any (reentrant).
 */
size_t kformat(kformat_emit_fn emit, void *ctx, const char *fmt, va_list args);

#endif
