# Manuals

The official documents for the hardware Anvil talks to. When a code comment says something
like `Arm ARM D24.2.174`, it means section D24.2.174 of the matching PDF here. Open the PDF and
search for that number.

The PDFs are not in git (Arm's license does not allow redistributing them). Download them with:

```sh
make manuals
```

Section numbers change between editions, so the code cites **exactly these editions**. When we
upgrade a manual, we re-check every citation in the code.

## CPU

| File | What it is | Used for |
|---|---|---|
| `DDI0487M_d_arm_architecture_reference_manual.pdf` | **Arm ARM**, Arm Architecture Reference Manual for A-profile, issue M.d (~15,000 pages). The rulebook for every ARM64 CPU. | Exception levels, system registers, memory types, MMU, exceptions |
| `aapcs64_2026Q3.pdf` | **AAPCS64**, Procedure Call Standard for Arm 64-bit, release 2026Q3. How C functions call each other. | Arguments in x0-x7, return value in x0, stack alignment |

Sections cited so far in the Arm ARM:

| Section | Topic | Where we use it |
|---|---|---|
| D1.1 | Exception levels (EL0-EL3) | `arch/aarch64/boot.S` |
| C5.2.2 | `CurrentEL`, bits [3:2] = current EL | `boot.S` (the `ubfx`) |
| C5.2.22 | `SPSR_EL2`: M[3:0], DAIF bits | `boot.S` (`SPSR_EL1H_MASKED`) |
| D24.2.61 | `HCR_EL2`: RW, bit 31 | `boot.S` (`HCR_EL2_RW`) |
| D24.2.174 | `SCTLR_EL1`: MMU/cache control, RES1 bits | `boot.S` (`SCTLR_EL1_MMU_OFF`) |
| D1.4.10.2 | SP alignment checking | `link.ld` |
| D8.2.12.1 | Memory type when the MMU is off | `arch/aarch64/mmio.c` |
| B2.10.2 | Device memory types (nGnRnE, ...) | `mmio.c` |
| D24.2.45 | `ESR_EL2.ISV` (why we avoid writeback forms on devices) | `mmio.c` |

AAPCS64: 6.1.1 (general-purpose registers, which ones hold arguments), 6.4.5.1 (stack
constraints, SP mod 16 = 0).

## Devices

| File | What it is | Used for |
|---|---|---|
| `DDI0183G_pl011_uart_trm.pdf` | **PL011 TRM**, PrimeCell UART Technical Reference Manual, r1p5. | `drivers/pl011.c`: 3.2 register list, 3.3.3 UARTFR flags, 3.3.7 UARTLCR_H format, 3.3.8 UARTCR control and the safe programming order |

## Firmware interfaces

| File | What it is | Used for |
|---|---|---|
| `DEN0022F_b_psci.pdf` | **PSCI**, Power State Coordination Interface, issue F.b. How the OS asks firmware to power off, reboot, or start other cores. | `arch/aarch64/power.c`: 5.1.9 SYSTEM_OFF function ID, 5.10 SYSTEM_OFF behaviour |

## Not a PDF

- **QEMU `virt` machine**: <https://www.qemu.org/docs/master/system/arm/virt.html>. What
  hardware the fake computer has. The exact addresses come from its device tree, which you can
  dump with `qemu-system-aarch64 -M virt -machine dumpdtb=virt.dtb` and read with
  `dtc -I dtb -O dts virt.dtb`.
