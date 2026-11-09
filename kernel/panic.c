#include <anvil/console.h>
#include <anvil/panic.h>
#include <arch/power.h>
#include <stdarg.h>
#include <stdbool.h>

/* Stops a panic while printing a panic (e.g. a KASSERT in the UART) from recursing. Single CPU,
 * IRQs masked, so a plain bool is enough. */
static bool panic_in_progress;

_Noreturn void kpanic(const char *fmt, ...)
{
    if (!panic_in_progress) {
        panic_in_progress = true;

        va_list args;
        va_start(args, fmt);
        kprintf("\nPANIC: ");
        vkprintf(fmt, args);
        kprintf("\n");
        va_end(args);
    }

    arch_poweroff();
}
