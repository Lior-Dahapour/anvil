#ifndef ANVIL_PANIC_H
#define ANVIL_PANIC_H

/**
 * Stop the kernel because of a kernel bug or a broken invariant: print "PANIC: <message>" to
 * the console, then power the machine off. Never returns.
 *
 * Only for bugs in the kernel itself. Errors caused by user input or by hardware must be
 * returned as error codes instead.
 *
 * @param fmt  printf-style message, same rules as kprintf().
 *
 * Context: any. Safe to call while a panic is already in progress (the second one does not
 * print, it only stops).
 */
_Noreturn void kpanic(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * Panic if `cond` is false, naming the condition and where it is written.
 *
 * A macro, not a function, so that #cond, __FILE__ and __LINE__ describe the caller's line.
 * The do/while(0) makes it behave like one statement, also inside an unbraced if/else.
 */
#define KASSERT(cond)                                                                              \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            kpanic("assertion failed: %s (%s:%d)", #cond, __FILE__, __LINE__);                     \
        }                                                                                          \
    } while (0)

#endif
