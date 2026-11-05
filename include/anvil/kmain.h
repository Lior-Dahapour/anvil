#ifndef ANVIL_KMAIN_H
#define ANVIL_KMAIN_H

/**
 * First C function, called by the architecture's boot code.
 *
 * Context: early boot, single CPU, EL1, MMU off, interrupts masked, .bss zeroed.
 */
_Noreturn void kmain(void);

#endif
