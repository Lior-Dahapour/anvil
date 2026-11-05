#include <anvil/panic.h>
#include <arch/mmio.h>

/*
 * Plain `ldr`/`str` through inline asm, instead of a C volatile pointer: the compiler may pick
 * load/store forms with address writeback, which hypervisors (including macOS HVF, if we ever
 * run with -accel hvf) cannot emulate on a device access because the CPU does not report the
 * register details for them (Arm ARM D24.2.45, ESR_EL2.ISV bit 24: no valid syndrome for
 * loads/stores "with writeback"). The asm pins the exact form.
 */

uint32_t mmio_read32(virt_addr_t base, size_t offset)
{
    KASSERT((offset & 3U) == 0U);

    uint32_t value = 0;
    uintptr_t addr = (uintptr_t)(base.value + offset);

    /*
     * SAFETY: the caller guarantees `base` is a device register block that is mapped as
     * Device memory (or the MMU is off, which makes every data access Device-nGnRnE, Arm ARM
     * D8.2.12.1), and that
     * `offset` is a 4-byte aligned register inside it. A single 32-bit load matches the
     * register width. "memory" clobber stops the compiler moving other memory accesses
     * across this one.
     */
    __asm__ volatile("ldr %w0, [%1]" : "=r"(value) : "r"(addr) : "memory");
    return value;
}

void mmio_write32(virt_addr_t base, size_t offset, uint32_t value)
{
    KASSERT((offset & 3U) == 0U);

    uintptr_t addr = (uintptr_t)(base.value + offset);

    /*
     * SAFETY: same requirements on `base` and `offset` as mmio_read32(). A single 32-bit
     * store matches the register width. Device-nGnRnE accesses to one device are not
     * reordered by the CPU (the "nR" attribute, Arm ARM B2.10.2.2); the "memory" clobber stops
     * the compiler doing so.
     */
    __asm__ volatile("str %w0, [%1]" : : "rZ"(value), "r"(addr) : "memory");
}
