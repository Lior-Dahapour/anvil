#ifndef ANVIL_PANIC_H
#define ANVIL_PANIC_H

/**
 * Print "PANIC: <message>" and power off. Only for kernel bugs: errors caused by input or
 * hardware are returned as error codes.
 *
 * @param fmt  printf-style message, kprintf() rules.
 *
 * Context: any. A panic during a panic does not print again.
 */
_Noreturn void kpanic(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/** Panic with the condition, file and line if `cond` is false. */
#define KASSERT(cond)                                                                              \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            kpanic("assertion failed: %s (%s:%d)", #cond, __FILE__, __LINE__);                     \
        }                                                                                          \
    } while (0)

#endif
