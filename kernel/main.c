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

/* TODO(dtb): take the UART from /chosen stdout-path. */
#define QEMU_VIRT_UART_BASE 0x09000000U

/* Linux Documentation/arch/arm64/booting.rst, section 2: the DTB is at most 2 MiB. */
#define DTB_MAX_SIZE (2U * 1024U * 1024U)
extern char __image_start[];
extern char __image_end[];
_Noreturn void kmain(void)
{
    /* MMU off: virtual = physical. */
    virt_addr_t uart = {.value = QEMU_VIRT_UART_BASE};

    pl011_init(uart);
    console_init(pl011_putc);

#ifdef ANVIL_SELFTEST
    selftest_run();
#endif

    kprintf("Anvil\n");

    phys_addr_t dtb = arch_boot_dtb_address();

    /*
     * SAFETY: MMU off, so the physical address works as a pointer. The boot protocol puts the
     * DTB in RAM, at most DTB_MAX_SIZE; fdt_find_memory never reads past its totalsize.
     */
    const uint8_t *blob = (const uint8_t *)(uintptr_t)dtb.value;
    phys_addr_t ram_base = {.value = 0};
    uint64_t ram_size = 0;
    if (fdt_find_memory(blob, DTB_MAX_SIZE, &ram_base, &ram_size) != KERR_OK) {
        kpanic("no RAM found in the device tree");
    }
    kprintf("RAM: 0x%lx - 0x%lx (%lu MiB)\n", ram_base.value, ram_base.value + ram_size,
            ram_size / (1024U * 1024U));
    uint32_t dtb_size = 0;
    if (fdt_total_size(blob, DTB_MAX_SIZE, &dtb_size) != KERR_OK) {
        kpanic("failed to determine the DTB size");
    }
    kprintf("Kernel: 0x%lx - 0x%lx\n", (uintptr_t)__image_start, (uintptr_t)__image_end);
    kprintf("DTB: 0x%lx (%u MiB)\n", (uintptr_t)dtb.value, dtb_size / (1024U * 1024U));

    arch_poweroff();
}
