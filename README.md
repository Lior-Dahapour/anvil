# Anvil

A small operating system kernel for 64-bit ARM (AArch64), written in C11 and assembly. It runs
on the QEMU `virt` machine. An x86_64 port is planned.

## Status

Done:

- Boots as an arm64 Image, drops from EL2 to EL1 if needed
- PL011 UART driver and `kprintf`
- Exception vectors with a register dump, `kpanic` and `KASSERT`
- Bounds-checked device tree parser that finds RAM
- Host unit tests under ASan/UBSan, and boot tests in QEMU

Next: physical memory manager, MMU and kernel heap, then interrupts, a scheduler, and
userspace.

## Build and run

Needs clang, `ld.lld`, LLVM binutils, `make` and QEMU. On macOS:

```sh
brew install qemu lld llvm
```

```sh
make          # build build/anvil.bin
make run      # boot in QEMU (quit with Ctrl-A X)
make test     # host unit tests
make check    # boot tests in QEMU
make help     # all targets
```

Output:

```
Anvil
RAM: 0x40000000 - 0x48000000 (128 MiB)
```

To debug, run `make debug`, then in another terminal `lldb build/anvil.elf -o 'gdb-remote 1234'`.

## Boot

1. QEMU loads `anvil.bin` at `0x40200000` and passes the device tree address in `x0`.
2. `arch/aarch64/boot.S` masks interrupts, drops to EL1, installs the exception vectors, sets
   up the stack, zeroes `.bss` and calls `kmain`.
3. `kmain` (`kernel/main.c`) starts the UART, reads the RAM range from the device tree, prints
   it and powers off through PSCI.

## Layout

```
arch/aarch64/   boot, exception vectors, MMIO, power off
include/        headers; include/arch/ is the interface each architecture implements
kernel/         kmain, kprintf, panic, device tree parser
drivers/        PL011 UART
tests/          host unit tests; tests/kernel/ has in-kernel crash tests
manuals/        reference manuals (`make manuals` downloads them)
```
