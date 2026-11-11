# Manuals

Code comments cite sections of these documents, e.g. `Arm ARM D24.2.174`. The PDFs are not in
git (Arm's license forbids redistribution). Download them with:

```sh
make manuals
```
## Not a PDF

- **Devicetree Specification v0.4**:
  <https://github.com/devicetree-org/devicetree-specification/releases/tag/v0.4>. Used by
  `kernel/fdt.c`.
- **QEMU `virt` machine**: <https://www.qemu.org/docs/master/system/arm/virt.html>. Dump its
  device tree with `qemu-system-aarch64 -M virt,dumpdtb=virt.dtb` and read it with
  `dtc -I dtb -O dts virt.dtb`.
