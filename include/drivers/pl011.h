#ifndef ANVIL_DRIVERS_PL011_H
#define ANVIL_DRIVERS_PL011_H

#include <anvil/types.h>

/**
 * Set up the PL011 UART (serial port) for output: 8 data bits, no parity, 1 stop bit.
 *
 * @param base  Address of the UART's register block, mapped as Device memory.
 *
 * Context: early boot, single CPU. Must be called before pl011_puts().
 */
void pl011_init(virt_addr_t base);

/**
 * Send a NUL-terminated string. "\n" is sent as "\r\n" so terminals start a new line.
 *
 * @param str  String to send. Must not be NULL.
 *
 * Context: early boot, single CPU (no locking yet).
 */
void pl011_puts(const char *str);

#endif
