#ifndef ANVIL_TESTS_SELFTEST_H
#define ANVIL_TESTS_SELFTEST_H

/**
 * Run the self-test chosen at build time (`make SELFTEST=NAME`), instead of the normal boot.
 * Each self-test deliberately ends in a panic; `make check` looks for that panic message.
 * Only built into self-test kernels.
 *
 * Context: early boot, after the console is set up.
 */
_Noreturn void selftest_run(void);

#endif
