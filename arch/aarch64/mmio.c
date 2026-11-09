#include <anvil/panic.h>
#include <arch/mmio.h>

/*
 * Inline asm instead of a volatile pointer, so the compiler cannot pick a load/store with
 * writeback: hypervisors cannot emulate those on MMIO (no valid syndrome, ESR_EL2.ISV, Arm ARM
 * D24.2.45).
 */

uint32_t mmio_read32(virt_addr_t base, size_t offset)
{
    KASSERT((offset & 3U) == 0U);

    uint32_t value = 0;
    uintptr_t addr = (uintptr_t)(base.value + offset);

    /*
     * SAFETY: caller passes a register block mapped as Device memory, or the MMU is off and all
     * accesses are Device-nGnRnE (Arm ARM D8.2.12.1). Offset is 4-byte aligned (asserted).
     * "memory" stops the compiler moving other accesses across this one.
     */
    __asm__ volatile("ldr %w0, [%1]" : "=r"(value) : "r"(addr) : "memory");
    return value;
}

void mmio_write32(virt_addr_t base, size_t offset, uint32_t value)
{
    KASSERT((offset & 3U) == 0U);

    uintptr_t addr = (uintptr_t)(base.value + offset);

    /*
     * SAFETY: as in mmio_read32(). The CPU does not reorder Device-nGnRnE accesses to one device
     * (Arm ARM B2.10.2.2); "memory" stops the compiler doing so.
     */
    __asm__ volatile("str %w0, [%1]" : : "rZ"(value), "r"(addr) : "memory");
}
