/* AI GENERATED TEST */
#ifndef ANVIL_TESTS_SELFTEST_H
#define ANVIL_TESTS_SELFTEST_H

/**
 * Run the self-test chosen by `make SELFTEST=NAME` instead of the normal boot. Each one ends in
 * a panic that `make check` looks for.
 *
 * Context: early boot, after console_init().
 */
_Noreturn void selftest_run(void);

#endif
