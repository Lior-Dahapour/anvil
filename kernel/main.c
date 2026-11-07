#include <anvil/console.h>
#include <anvil/kmain.h>
#include <anvil/types.h>
#include <arch/power.h>
#include <drivers/pl011.h>

#ifdef ANVIL_SELFTEST
#include <selftest.h>
#endif

/* QEMU `virt` PL011 UART. TODO(dtb): take this from the device tree's /chosen stdout-path. */
#define QEMU_VIRT_UART_BASE 0x09000000U

_Noreturn void kmain(void)
{
    /* MMU is off, so the physical register address is also the address we use. */
    virt_addr_t uart = {.value = QEMU_VIRT_UART_BASE};

    pl011_init(uart);
    console_init(pl011_putc);

#ifdef ANVIL_SELFTEST
    selftest_run();
#endif

    kprintf("Anvil\n");

    arch_poweroff();
}
