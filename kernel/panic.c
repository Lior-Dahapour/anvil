#include <anvil/console.h>
#include <anvil/panic.h>
#include <arch/power.h>
#include <stdarg.h>
#include <stdbool.h>

/*
 * Set when the first panic starts. If printing the message fails an assertion of its own (for
 * example in the console driver), that second kpanic() must not try to print again, or it
 * would recurse forever. Single CPU with interrupts masked, so a plain bool is enough.
 */
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
