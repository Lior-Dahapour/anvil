# Anvil

A hobby operating system kernel in C, built step by step to learn how computers work at the
lowest level. The first target is 64-bit ARM (AArch64) running in the QEMU emulator.

Current state: **Milestone 1**. The kernel boots, prints `Anvil` over the serial port, and
powers the machine off.

## What you need

macOS with Homebrew:

```sh
brew install qemu lld
```

- `qemu`: the emulator, a fake ARM computer that runs our kernel.
- `lld`: the linker, which glues compiled files into one kernel file.
- `clang` (the compiler) and `make` already come with macOS (Xcode command line tools).

## Build and run

```sh
make          # build build/anvil.bin (and build/anvil.elf, for the debugger)
make run      # boot it in QEMU; you should see "Anvil", then QEMU exits by itself
make check    # automatic tests: normal boot at EL1 and EL2, plus crash self-tests, PASS/FAIL
make test     # unit tests that run on the Mac (kprintf formatting)

make run SELFTEST=BRK         # boot a kernel that crashes on purpose and see the register dump
                              # (also UDF, UNALIGNED, ASSERT; see tests/kernel/selftest.c)
make help     # all targets
```

If the kernel ever hangs inside `make run`, quit QEMU with `Ctrl-A` then `X`.

## How it boots

1. **QEMU starts.** It pretends to be an ARM computer called `virt` with 128 MiB of RAM at
   address `0x40000000` and a serial port (a PL011 UART) at `0x09000000`.
2. **QEMU loads `build/anvil.bin`** into RAM at `0x40200000`. This is a raw kernel image: the
   ELF file stripped down to plain bytes, starting with a 64-byte header that tells QEMU "this
   is an ARM64 kernel". For this format QEMU also copies the device tree (a description of
   the machine: RAM, devices) into RAM and passes its address in register `x0`.
3. **The CPU starts at `_start` in `arch/aarch64/boot.S`.** At this point nothing is set up:
   no stack, and C code cannot run yet. This assembly code:
   - jumps over the image header and saves the device tree address from `x0`;
   - turns interrupts off, because we can't handle them yet;
   - checks the privilege level (the "exception level"), and if the CPU started at EL2
     (hypervisor level), drops down to EL1, the level where kernels run;
   - points the stack pointer at a 16 KiB stack reserved in `link.ld`;
   - fills the `.bss` area with zeros, because C expects uninitialized globals to start at 0;
   - calls `kmain()`.
4. **`kmain()` in `kernel/main.c`** sets up the serial port (`drivers/pl011.c`), prints
   `Anvil`, and asks the firmware to power off (`arch/aarch64/power.c`), which makes QEMU exit.

## Layout

```
arch/aarch64/   code that only makes sense on ARM (boot, power off, register access)
include/        headers; include/arch/ is what every CPU architecture must provide
kernel/         architecture-neutral kernel code
drivers/        device drivers (the PL011 serial port)
manuals/        hardware manuals (PDFs downloaded by `make manuals`) and what we use from each
link.ld         where each part of the kernel goes in memory
CLAUDE.md       coding rules for this repo
```

## Debugging

```sh
make debug                                   # QEMU waits, frozen, at the first instruction
lldb build/anvil.elf -o 'gdb-remote 1234'    # in a second terminal
```
