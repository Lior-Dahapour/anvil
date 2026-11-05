#include <arch/mmio.h>
#include <drivers/pl011.h>
#include <stdint.h>

/* Register offsets and bits: PL011 TRM (DDI 0183), 3.2 "Summary of registers". */
#define UART_DR 0x000U   /* Data register: write a byte here to send it. */
#define UART_FR 0x018U   /* Flag register. */
#define UART_LCRH 0x02CU /* Line control: data format. */
#define UART_CR 0x030U   /* Control: enable bits. */
#define UART_IMSC 0x038U /* Interrupt mask. */

#define FR_BUSY (1U << 3) /* Still sending a byte. */
#define FR_TXFF (1U << 5) /* Transmit FIFO full: wait before writing DR. */

#define LCRH_FEN (1U << 4)    /* Enable FIFOs. */
#define LCRH_WLEN_8 (3U << 5) /* 8 data bits. No parity, 1 stop bit are the zero defaults. */

#define CR_UARTEN (1U << 0) /* UART enable. */
#define CR_TXE (1U << 8)    /* Transmit enable. */
#define CR_RXE (1U << 9)    /* Receive enable. */

/* Set once by pl011_init() during single-CPU early boot, read-only afterwards. */
static virt_addr_t uart_base;

void pl011_init(virt_addr_t base)
{
    uart_base = base;

    /*
     * Programming order from the TRM, 3.3.8 (UARTCR): disable the UART, wait for the byte in
     * flight to finish, then change the format, then enable again.
     * The baud rate (IBRD/FBRD) is not set: it depends on the UART clock from the device tree,
     * and QEMU ignores it. TODO(dtb): set the baud rate from the "uartclk" clock.
     */
    mmio_write32(uart_base, UART_CR, 0U);
    while ((mmio_read32(uart_base, UART_FR) & FR_BUSY) != 0U) {
    }
    mmio_write32(uart_base, UART_IMSC, 0U);
    mmio_write32(uart_base, UART_LCRH, LCRH_WLEN_8 | LCRH_FEN);
    mmio_write32(uart_base, UART_CR, CR_UARTEN | CR_TXE | CR_RXE);
}

static void pl011_putc(char c)
{
    while ((mmio_read32(uart_base, UART_FR) & FR_TXFF) != 0U) {
    }
    mmio_write32(uart_base, UART_DR, (uint32_t)(unsigned char)c);
}

void pl011_puts(const char *str)
{
    for (const char *p = str; *p != '\0'; p++) {
        if (*p == '\n') {
            pl011_putc('\r');
        }
        pl011_putc(*p);
    }
}
