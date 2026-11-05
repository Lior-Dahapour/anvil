#ifndef ANVIL_DRIVERS_PL011_H
#define ANVIL_DRIVERS_PL011_H

#include <anvil/types.h>

/**
 * Set up the PL011 UART (serial port) for output: 8 data bits, no parity, 1 stop bit.
 *
 * @param base  Address of the UART's register block, mapped as Device memory.
 *
 * Context: early boot, single CPU. Must be called before pl011_putc().
 */
void pl011_init(virt_addr_t base);

/**
 * Send one byte, waiting while the transmit FIFO is full. No newline translation.
 *
 * @param c  The byte to send.
 *
 * Context: single CPU (no locking yet). Must be called after pl011_init().
 */
void pl011_putc(char c);

#endif
