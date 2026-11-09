#ifndef ANVIL_DRIVERS_PL011_H
#define ANVIL_DRIVERS_PL011_H

#include <anvil/types.h>

/**
 * Set up the PL011 UART for output, 8N1.
 *
 * @param base  The UART's register block, mapped as Device memory.
 *
 * Context: early boot, single CPU, before pl011_putc().
 */
void pl011_init(virt_addr_t base);

/**
 * Send one byte, waiting while the TX FIFO is full. No newline translation.
 *
 * @param c  The byte.
 *
 * Context: single CPU, no locking yet. After pl011_init().
 */
void pl011_putc(char c);

#endif
