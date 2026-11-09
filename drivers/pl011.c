#include <anvil/panic.h>
#include <arch/mmio.h>
#include <drivers/pl011.h>
#include <stdint.h>

/* Registers and bits: PL011 TRM (DDI 0183), 3.2 "Summary of registers". */
#define UART_DR 0x000U   /* data */
#define UART_FR 0x018U   /* flags */
#define UART_LCRH 0x02CU /* line control */
#define UART_CR 0x030U   /* control */
#define UART_IMSC 0x038U /* interrupt mask */

#define FR_BUSY (1U << 3)
#define FR_TXFF (1U << 5) /* TX FIFO full */

#define LCRH_FEN (1U << 4)
#define LCRH_WLEN_8 (3U << 5) /* 8 data bits; no parity and 1 stop bit are the zero defaults */

#define CR_UARTEN (1U << 0)
#define CR_TXE (1U << 8)
#define CR_RXE (1U << 9)

/* Set once by pl011_init() in early boot, read-only after. */
static virt_addr_t uart_base;

void pl011_init(virt_addr_t base)
{
    uart_base = base;

    /*
     * Order from TRM 3.3.8: disable, wait until idle, flush the FIFO (FEN = 0), set the format,
     * enable. Baud rate is left alone, QEMU ignores it. TODO(dtb): set it from "uartclk".
     */
    mmio_write32(uart_base, UART_CR, 0U);
    while ((mmio_read32(uart_base, UART_FR) & FR_BUSY) != 0U) {
    }
    mmio_write32(uart_base, UART_LCRH, 0U);
    mmio_write32(uart_base, UART_IMSC, 0U);
    mmio_write32(uart_base, UART_LCRH, LCRH_WLEN_8 | LCRH_FEN);
    mmio_write32(uart_base, UART_CR, CR_UARTEN | CR_TXE | CR_RXE);
}

void pl011_putc(char c)
{
    KASSERT(uart_base.value != 0U);

    while ((mmio_read32(uart_base, UART_FR) & FR_TXFF) != 0U) {
    }
    mmio_write32(uart_base, UART_DR, (uint32_t)(unsigned char)c);
}
