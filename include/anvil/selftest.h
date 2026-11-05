#ifndef ANVIL_SELFTEST_H
#define ANVIL_SELFTEST_H

#ifdef ANVIL_SELFTEST
/**
 * Run the self-test chosen at build time (`make SELFTEST=NAME`), instead of the normal boot.
 * Each self-test deliberately ends in a panic; `make check` looks for that panic message.
 * Only exists in self-test kernels.
 *
 * Context: early boot, after the console is set up.
 */
_Noreturn void selftest_run(void);
#endif

#endif
