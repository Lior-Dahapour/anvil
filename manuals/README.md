# Manuals

Code comments cite sections of these documents, e.g. `Arm ARM D24.2.174`. The PDFs are not in
git (Arm's license forbids redistribution). Download them with:

```sh
make manuals
```

Section numbers change between editions; the code cites exactly these.

## CPU

| File | What it is | Used for |
|---|---|---|
| `DDI0487M_d_arm_architecture_reference_manual.pdf` | **Arm ARM**, Arm Architecture Reference Manual for A-profile, issue M.d | Exception levels, system registers, memory types, MMU, exceptions |
| `aapcs64_2026Q3.pdf` | **AAPCS64**, Procedure Call Standard for Arm 64-bit, release 2026Q3 | Calling convention, stack alignment |

## Devices

| File | What it is | Used for |
|---|---|---|
| `DDI0183G_pl011_uart_trm.pdf` | **PL011 TRM**, PrimeCell UART Technical Reference Manual, r1p5 | `drivers/pl011.c` |

## Firmware interfaces

| File | What it is | Used for |
|---|---|---|
| `DEN0022F_b_psci.pdf` | **PSCI**, Power State Coordination Interface, issue F.b | `arch/aarch64/power.c` |

## Not a PDF

- **Devicetree Specification v0.4**:
  <https://github.com/devicetree-org/devicetree-specification/releases/tag/v0.4>. Used by
  `kernel/fdt.c`.
- **QEMU `virt` machine**: <https://www.qemu.org/docs/master/system/arm/virt.html>. Dump its
  device tree with `qemu-system-aarch64 -M virt,dumpdtb=virt.dtb` and read it with
  `dtc -I dtb -O dts virt.dtb`.
