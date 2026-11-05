#ifndef ANVIL_FORMAT_H
#define ANVIL_FORMAT_H

#include <stdarg.h>
#include <stddef.h>

/**
 * Receives each output character of kformat(), in order.
 *
 * @param c    The character.
 * @param ctx  The `ctx` pointer passed to kformat(), unchanged.
 */
typedef void (*kformat_emit_fn)(char c, void *ctx);

/**
 * printf-style formatting, without any output device: every character is handed to `emit`.
 *
 * Supported conversions: %c %s %d %i %u %x %X %p %%. Length modifiers: l, ll, z. An optional
 * width with optional '0' flag, for example %5d or %016lx. %p prints "0x" and 16 hex digits.
 * A NULL %s prints "(null)". Anything unsupported is printed as written.
 *
 * Has no global state and allocates nothing, so it also compiles and runs on the host for
 * unit tests (`make test`).
 *
 * @param emit  Character sink. Must not be NULL.
 * @param ctx   Passed through to `emit`. May be NULL.
 * @param fmt   Format string. Must not be NULL.
 * @param args  Arguments matching `fmt`.
 *
 * @return Number of characters passed to `emit`.
 *
 * Context: any (reentrant).
 */
size_t kformat(kformat_emit_fn emit, void *ctx, const char *fmt, va_list args);

#endif
