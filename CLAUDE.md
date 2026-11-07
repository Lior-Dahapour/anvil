# Anvil: C Kernel Coding Guide

Rules for all code in this repo, for AI agents and humans alike. Anvil is a freestanding kernel
in C11. The first target is **AArch64 (ARM64)** running under QEMU `virt`; x86_64 comes later as a
second port. Rust crates arrive late, behind a small FFI boundary. A kernel has no safety net:
one bad pointer is a fault or silent corruption, so the code must make its assumptions explicit
and checkable.

## Core Principles

1. **Correctness first, then clarity, then speed.** Do not optimize without a measured reason.
2. **Every assumption is written down.** If code relies on something the compiler cannot verify
   (pointer validity, alignment, memory attributes, barriers, locking, interrupt state, mapping),
   it gets a `SAFETY:` comment and, where cheap, a runtime assertion.
3. **No extra code.** Solve the current milestone only. No speculative abstractions, no dead code.
4. **Small, auditable unsafe surface.** Raw pointer and system-register work is concentrated in a
   few files (`arch/`, `mm/`, `drivers/`) behind typed, checked helpers. The rest of the kernel
   calls the helpers.
5. **Architecture-neutral core.** Anything specific to AArch64 lives under `arch/aarch64/`. Core
   code (`kernel/`, `mm/`, `fs/`) talks to it through headers in `include/arch/`, so the later
   x86_64 port adds `arch/x86_64/` instead of rewriting the kernel.
6. Ask before big design decisions, otherwise proceed without asking for clarification.

## Toolchain and Build

- C11, freestanding, no libc. Compiler: clang with `--target=aarch64-none-elf` and `ld.lld`, or a
  `aarch64-none-elf` cross gcc. Build with `make`.
- Run and test: `qemu-system-aarch64 -M virt -cpu cortex-a72 -nographic -kernel build/anvil.bin`
  (add `-s -S` and use `gdb-multiarch` for debugging).
- Required flags:
  `-std=c11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mgeneral-regs-only
  -mstrict-align -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion
  -Wstrict-prototypes -Wmissing-prototypes -Wcast-align -Wpointer-arith -Wvla -Wundef`
  Link with `-nostdlib -static` and our own linker script.
- `-mgeneral-regs-only`: no floating point or SIMD registers in kernel code (their state is not
  saved on exceptions). `-mstrict-align`: with the MMU off all accesses are Device memory, where
  unaligned accesses fault, so the compiler must never generate them in early boot.
- No VLAs, no `alloca`, no floating point, no recursion in exception paths.
- Use `clang-format` (checked in `.clang-format`) and `clang-tidy`/`cppcheck` where available.
- Debug builds enable `KASSERT` and, once a runtime exists, UBSan. Release keeps `KASSERT` for
  invariants that guard memory safety.

## Layout

```
anvil/
  arch/aarch64/   boot.S, vectors.S, mmu, exceptions, gic, timer, context switch, sysreg helpers
  include/        public headers, and include/arch/ (the interface each arch implements)
  kernel/         scheduler, syscalls, panic, printk
  mm/             pmm, vmm, kmalloc
  drivers/        pl011 uart, gic, timer, virtio
  userland/       later
  rust/           later (anvil-sys, fs, drivers)
  Makefile, link.ld, README.md
```

## Style and Naming

- 4 spaces, no tabs. Line length 100. K&R braces. Always use braces, even for one-line bodies.
- `snake_case` for functions, variables, and files. `UPPER_SNAKE_CASE` for macros and constants.
  Types end in `_t` only for typedefs of scalars or opaque handles; structs are `struct foo`.
- Prefix public functions with their module: `pmm_alloc_frame`, `vmm_map_page`, `sched_yield`.
- Functions that are file-local are `static`. Anything non-static must be declared in a header.
- Fixed-width types (`uint32_t`, `uint64_t`) for hardware and ABI data. `size_t` for sizes,
  `uintptr_t` for pointer-as-integer. Never `int` or `long` for a register or a struct that
  mirrors hardware.
- No emoji or emoji-like unicode in code or comments.
- Prefer early return over deep nesting. Keep functions short and single-purpose.
- One declaration per line. Initialize every variable at declaration.
- Comments explain **why**, never what the code obviously does. Do not restate the function name.
  Do not leave commented-out code.

## SAFETY Comments (mandatory)

A `SAFETY:` comment is required at **every** place where correctness depends on something the
compiler cannot check. This is the most important rule in this guide.

### When a SAFETY comment is required

- Dereferencing a pointer whose validity is not proven in the same function (anything that came
  from a parameter, a global, a hardware table, the device tree, a physical-address conversion,
  or a cast).
- Pointer arithmetic, indexing through a pointer, and `container_of`.
- Casting between pointer types, or between integers and pointers (alignment, strict aliasing,
  signedness).
- Casts that discard `const` or `volatile`.
- MMIO reads and writes (state the mapping, the memory attribute, and the register width).
- System register access (`mrs`/`msr`): state the exception level required and the side effects.
- Memory barriers and cache or TLB maintenance, or the **absence** of one where ordering matters
  (`dmb`, `dsb`, `isb`, `dc`, `ic`, `tlbi`).
- Inline assembly (state clobbers, memory effects, exception level, interrupt state).
- Physical-to-virtual conversion through the direct map.
- Type punning of hardware or wire structures (`packed` structs, misaligned access).
- Code that assumes a lock is held, interrupts are masked, a specific exception level, or a
  specific context (IRQ, process, early boot with MMU off).
- Any `memcpy`/`memset`/`memmove` where the caller guarantees sizes or non-overlap.
- Lifetime assumptions: the object outlives the pointer, or the memory is never freed or
  remapped while in use.

### What a SAFETY comment must contain

State the **specific facts** that make the operation valid. Check them against: non-null,
alignment, mapped and accessible, correct memory attribute (Normal vs Device), in-bounds,
initialized, lifetime, exclusive access or synchronization, ordering (barriers), and context
(exception level, interrupts, locks). Say who guarantees each fact (the caller, an earlier
check, the hardware, the bootloader, or the device tree). "This is safe" or "pointer is valid"
alone is not acceptable.

Format:

```c
/*
 * SAFETY: <fact 1>; <fact 2>. Guaranteed by <who/what>.
 */
```

Good:

```c
static uint64_t read_pte(const struct pt_table *table, size_t index)
{
    KASSERT(table != NULL);
    KASSERT(index < PT_ENTRIES);

    /*
     * SAFETY: `table` points at a 4 KiB translation-table page mapped as Normal memory through
     * the direct map and kept mapped for the life of the address space. `index` < 512 is
     * asserted above, so the access is in-bounds. Entries are naturally aligned 8-byte values.
     * Caller holds the address space lock, so there is no concurrent writer.
     */
    return table->entries[index];
}
```

Good (MMIO helper, the only place raw device pointers are dereferenced):

```c
static inline void mmio_write32(virt_addr_t base, size_t offset, uint32_t value)
{
    KASSERT((offset & 3U) == 0U);

    /*
     * SAFETY: `base` is a mapping of a device register block created with Device-nGnRE
     * attributes (never cached, never reordered with other Device accesses to this device).
     * `offset` is 4-byte aligned and inside the block per the device's register map. The
     * volatile access forces a single 32-bit store; ordering against Normal memory is the
     * caller's job via mmio_wmb().
     */
    *(volatile uint32_t *)(uintptr_t)(base.value + offset) = value;
}
```

Bad (rejected in review):

```c
/* SAFETY: this is fine. */
return *ptr;
```

### Functions that trust their caller: `_unchecked`

A function that does not validate its input, and is only correct if the caller upholds a
precondition it cannot check itself, gets the suffix `_unchecked` (Rust's convention, for
example `read_be32_unchecked`). Its doc comment has a `Safety:` line stating exactly what the
caller must guarantee, and every call site has a `SAFETY:` comment saying why that holds there.
A function that validates its own input keeps a plain name. The hardware access helpers
(`mmio_*`, `read_sysreg`, barriers) are exempt: their contract is documented once in their
header, see "Hardware access helpers".

### Pair with a runtime check when it is cheap

If a SAFETY claim can be checked in O(1) (non-null, alignment, range, lock held, interrupts
masked, current exception level), add a `KASSERT` next to it. The comment documents the intent,
the assertion enforces it. Use project helpers:

- `KASSERT(cond)`: panics with file/line and a register dump when false.
- `KASSERT_ALIGNED(ptr, align)`, `KASSERT_IRQS_OFF()`, `KASSERT_LOCK_HELD(lock)`, `KASSERT_EL(n)`.
- `KUNREACHABLE()`: for impossible paths, never silently fall through.

### Hardware access helpers

- MMIO goes through `mmio_read32/64` and `mmio_write32/64` (volatile, with the SAFETY comment
  inside the helper). Driver code does not dereference raw MMIO pointers directly.
- System registers go through `read_sysreg(name)` / `write_sysreg(name, value)`. Barriers go
  through `dmb_ish()`, `dsb_sy()`, `isb()`. Each helper documents the required exception level
  and side effects once, callers then only note *why this value is correct*.
- Hardware constants (register bits, field layouts, descriptor flags) get a comment citing the
  manual and section, for example `Arm ARM (DDI 0487), D8 (VMSAv8-64 translation)`, `PL011 TRM
  (DDI 0183)`, `GIC Architecture Spec (IHI 0048 for v2, IHI 0069 for v3)`.
- Memory attributes (Normal vs Device-nGnRnE/nGnRE) are chosen via `MAIR_EL1` indices defined
  in one place. Never hardcode an attribute index at a use site.

## AArch64 Rules

- **Exception levels:** the kernel runs at EL1, userspace at EL0. If entered at EL2 (real
  hardware or `virtualization=on`), the boot code drops to EL1 before any C code runs.
- **MMU off in early boot:** while the MMU is off, all data accesses are Device memory. Do not
  use unaligned accesses, spinlocks, or atomics (exclusive load/store) until the MMU and caches
  are enabled. Early boot is single-core.
- **Stack:** SP must be 16-byte aligned at all times. Each exception level and each CPU has its
  own stack. State which stack (`SP_EL0` vs `SP_EL1`, `SPSel`) the code assumes.
- **Exception vectors:** `VBAR_EL1` table is 2 KiB aligned, 16 entries of 128 bytes. Handlers
  save and restore the full register context and never touch FP/SIMD registers. The panic dump
  prints `ELR_EL1`, `ESR_EL1` (decoded EC), `FAR_EL1`, `SPSR_EL1`, and `x0`-`x30`.
- **Interrupt masking:** use `DAIF` helpers (`irq_save`/`irq_restore`), never open-coded `msr`
  at call sites.
- **Barriers:** after changing translation tables, the sequence is
  `dsb ishst; tlbi ...; dsb ish; isb`, with a SAFETY comment stating which one is needed and why.
  After writing code that will be executed, clean the D-cache and invalidate the I-cache.
- **Caches and DMA:** buffers shared with devices (virtio rings) are either mapped
  non-cacheable or cleaned/invalidated explicitly. State which in the SAFETY comment.
- **Page tables:** 4 KiB granule, 48-bit VA. `TTBR1_EL1` for the kernel (higher half),
  `TTBR0_EL1` for userspace. Set `WXN` and mark data `PXN`/`UXN` so memory is never both writable
  and executable.
- **Device tree:** hardware addresses and IRQ numbers come from the DTB passed in `x0`. Hardcoded
  QEMU `virt` values are allowed only during early bring-up, marked `TODO(dtb)`, and must be
  replaced by DTB parsing by Milestone 4. The DTB is untrusted-format input: validate magic,
  sizes, and offsets before reading.
- **Secondary CPUs:** brought up through PSCI (`hvc`/`smc`) only after the single-core kernel is
  stable. Do not design for SMP before then, but do not add assumptions that block it.

## Types and API Design

- Use distinct types for distinct meanings so the compiler catches mixups. Physical and virtual
  addresses must not be interchangeable:

  ```c
  typedef struct { uint64_t value; } phys_addr_t;
  typedef struct { uint64_t value; } virt_addr_t;
  ```

- Mark ownership in names or comments: who allocates, who frees, who may mutate.
- Pass sizes together with pointers. Prefer `(const uint8_t *buf, size_t len)` pairs, or a small
  `struct slice`.
- Use `const` everywhere it applies. Use `restrict` only when non-aliasing is guaranteed and
  documented with a SAFETY comment.
- Use `enum` for states and error codes, and `_Static_assert` for layout and size assumptions
  about hardware structs (`sizeof`, `offsetof`).
- Hardware and wire structs use `__attribute__((packed))` only when the format requires it, and
  access to misaligned members goes through `memcpy` or a helper.
- Avoid global mutable state. When unavoidable, mark it `static`, document which lock or context
  protects it, and keep it in the owning module.
- Opaque structs in headers for anything with invariants; expose functions, not fields.

## Error Handling

- Fallible functions return `kerr_t` (negative error code or `KERR_OK`), with results returned
  through out-parameters. Mark them so ignoring the result is a compile error:
  `#define KMUST_CHECK __attribute__((warn_unused_result))`.
- Never ignore a return value. Propagate with the `KTRY(expr)` macro or handle explicitly.
- Use `NULL` only to mean "no object". Do not use magic integers as sentinels when an enum or a
  separate flag is clearer.
- `kpanic(...)` is for unrecoverable kernel bugs and broken invariants only, never for
  conditions caused by user input or hardware flakiness.
- Validate everything that crosses a trust boundary (userspace pointers, syscall arguments, the
  DTB, disk and network data, ELF headers) before use. Userspace pointers are only touched
  through `copy_from_user` / `copy_to_user`, never dereferenced directly.
- Integer safety: check for overflow before arithmetic on sizes and addresses
  (`__builtin_add_overflow`, `__builtin_mul_overflow`). Be explicit with signed/unsigned
  conversions.

## Memory

- Allocation goes through `pmm_*` (frames), `vmm_*` (mappings), and `kmalloc/kfree` (heap).
  Every allocation has one documented owner and one free path.
- Check every allocation result. Initialize allocated memory before use (`kzalloc` when in doubt).
- Set pointers to `NULL` after free. No use-after-free, no double free; in debug builds poison
  freed memory.
- Stack space is tiny. Keep large objects off the stack, and never put buffers larger than about
  512 bytes in a kernel stack frame.
- Do not allocate in interrupt context unless the allocator is documented as IRQ-safe.
- Zero uninitialized padding before copying structs to userspace.

## Concurrency and Interrupts

- AArch64 has a **weak memory model**. Do not assume stores become visible in program order to
  other cores or to devices. Use acquire/release atomics (`__atomic_*` builtins) for shared
  state, and explicit barriers for device ordering, with a comment stating the ordering needed.
- Every function that touches shared state documents its context in a header comment:
  `Context: any | process only | IRQ-safe | requires lock X | requires IRQs masked`.
- Locks: use an irqsave spinlock when data is shared with interrupt handlers. Define and document
  a global lock order, and never acquire locks out of order.
- Never block or yield while holding a spinlock. Never call a possibly-sleeping function from an
  interrupt handler.
- Keep interrupt handlers short: acknowledge (EOI), record, defer.
- Per-CPU data lives behind `TPIDR_EL1` and is accessed only with preemption or IRQs masked, with
  a SAFETY comment saying why the CPU cannot change underneath.
- `volatile` is for MMIO and hardware-shared memory only, never as a synchronization tool.

## Documentation

- Every non-static function in a header has a doc comment covering: purpose, parameters
  (ownership, nullability, units), return value and error codes, and **Context**.
- Comment struct fields that mirror hardware with the register or table they belong to.
- Keep comments in sync with code. A wrong comment is worse than none.

Example:

```c
/**
 * Map one 4 KiB page into an address space.
 *
 * @param as     Target address space. Must not be NULL.
 * @param virt   Page-aligned virtual address to map.
 * @param phys   Page-aligned physical frame. Ownership stays with the caller.
 * @param flags  Combination of VMM_WRITE, VMM_USER, VMM_EXEC.
 *
 * @return KERR_OK on success, KERR_NOMEM if a table frame could not be allocated,
 *         KERR_EXISTS if `virt` is already mapped.
 *
 * Context: process only, takes `as->lock`.
 */
KMUST_CHECK kerr_t vmm_map_page(struct address_space *as, virt_addr_t virt, phys_addr_t phys,
                                uint32_t flags);
```

## Testing

- Pure logic (allocator data structures, parsers, bitmaps, list helpers, DTB and filesystem code)
  is written so it compiles on the host and is unit tested with plain `make test`.
- Hardware-dependent code is tested by booting under `qemu-system-aarch64 -M virt` with a serial
  test harness. Tests report pass/fail over the UART and exit QEMU via PSCI `SYSTEM_OFF` or
  semihosting. A milestone is done only when it boots and passes its QEMU test.
- Add tests for every new function that has logic, using Arrange-Act-Assert. Include error paths
  and boundary values (zero, max, off-by-one, unaligned).
- Run host tests under ASan and UBSan. Fuzz parsers (DTB, ELF loader, filesystem).
- Do not commit commented-out or disabled tests.

## Milestones

1. Boots under QEMU `virt`, drops to EL1 if needed, prints over the PL011 UART, halts cleanly.
   `make run` works.
2. `kprintf`, panic with register dump, exception vectors and handlers (synchronous, IRQ, SError).
3. DTB parsing, physical memory manager, MMU setup (TTBR0/TTBR1), kernel heap.
4. GIC and generic timer interrupts. Remove all `TODO(dtb)` hardcoding.
5. Kernel threads, context switch, round-robin scheduler.
6. `svc` syscalls, EL0 userspace, ELF loader, first `init`.
7. virtio-blk driver, then filesystem (this is where Rust enters).

## Rust Boundary (later milestone)

- Rust crates target `aarch64-unknown-none-softfloat`, built as `no_std` static libraries linked
  into the C kernel. The boundary has roughly 10 functions each way.
- Everything crossing it is `extern "C"`, `#[repr(C)]` on the Rust side, with headers generated
  by `cbindgen` so they cannot drift.
- Every C function Rust calls must document its preconditions as a SAFETY contract. Every
  `extern "C"` entry point exposed to C documents the pointer, length, and lifetime rules the
  caller must uphold.
- Do not add Rust before Milestone 7.

## Version Control

- Small commits, one logical change each, with a clear message (imperative mood, explains why).
- Never commit debug prints, commented-out code, build outputs, or secrets.
- Each commit must build with zero warnings.

## Agent Behavior

- Do not create unnecessary scripts or files that pollute the repo.
- Implement the current milestone only. Do not build ahead.
- Before the first build, check that `qemu-system-aarch64`, an AArch64 bare-metal toolchain
  (clang + lld, or `aarch64-none-elf-gcc`), `make`, and `gdb-multiarch` are installed, and report
  anything missing. `dtc` is optional, for inspecting the device tree.
- When launching subagents, start each in its own parallel call.
- Prefer reading specific files over broad exploration, and never read build output or generated
  files.

## Before Committing

- [ ] Builds with zero warnings (`make`)
- [ ] `make test` passes (host unit tests, sanitizers clean)
- [ ] Boots in QEMU and passes the milestone test (`make run` / `make check`)
- [ ] `clang-format` clean, `clang-tidy` clean
- [ ] Every raw pointer deref, cast, MMIO access, system register access, barrier decision, and
  inline asm block that needs one has a specific `SAFETY:` comment, and cheap invariants
  have a `KASSERT`
- [ ] Every public function has a doc comment including **Context**
- [ ] All return values are checked, no unchecked allocations
- [ ] AArch64-specific code stays under `arch/aarch64/`, core code stays architecture-neutral
- [ ] No commented-out code, debug prints, or hardcoded secrets

---

**Remember:** if you cannot write down why a pointer access is valid, the code is not ready.