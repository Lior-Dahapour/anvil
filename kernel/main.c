#include <anvil/console.h>
#include <anvil/fdt.h>
#include <anvil/kmain.h>
#include <anvil/panic.h>
#include <anvil/types.h>
#include <arch/boot.h>
#include <arch/power.h>
#include <drivers/pl011.h>

#ifdef ANVIL_SELFTEST
#include <selftest.h>
#endif

/* QEMU `virt` PL011 UART. TODO(dtb): take this from the device tree's /chosen stdout-path. */
#define QEMU_VIRT_UART_BASE 0x09000000U

/* Linux Documentation/arch/arm64/booting.rst, section 2: the DTB is at most 2 MiB. */
#define DTB_MAX_SIZE (2U * 1024U * 1024U)

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

    phys_addr_t dtb = arch_boot_dtb_address();

    /*
     * SAFETY: the MMU is off, so a physical address is used directly as a pointer. The arm64
     * boot protocol puts the DTB in RAM, 8-byte aligned and at most 2 MiB (DTB_MAX_SIZE), and
     * fdt_find_memory never reads past the header's totalsize, which it checks is <= that.
     */
    const uint8_t *blob = (const uint8_t *)(uintptr_t)dtb.value;
    phys_addr_t ram_base = {.value = 0};
    uint64_t ram_size = 0;
    if (fdt_find_memory(blob, DTB_MAX_SIZE, &ram_base, &ram_size) != KERR_OK) {
        kpanic("no RAM found in the device tree");
    }
    kprintf("RAM: 0x%lx - 0x%lx (%lu MiB)\n", ram_base.value, ram_base.value + ram_size,
            ram_size / (1024U * 1024U));

    arch_poweroff();
}
