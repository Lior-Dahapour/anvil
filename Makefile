# Anvil build. Run `make help` for the list of targets.

ARCH ?= aarch64

# SELFTEST=NAME builds a test kernel that runs tests/kernel/selftest.c instead of booting
# normally, into its own folder so it never mixes with the normal build. Used by `make check`.
SELFTEST ?=
BUILD := build$(if $(SELFTEST),/selftest-$(SELFTEST))
KERNEL := $(BUILD)/anvil.elf
# Raw image (ELF stripped to plain bytes) that QEMU boots: only for this format does it pass the
# device tree address in x0. The ELF stays for lldb, which needs its symbols.
IMAGE := $(BUILD)/anvil.bin

CC := clang
LD := ld.lld
QEMU := qemu-system-$(ARCH)
CLANG_FORMAT ?= $(firstword $(wildcard /opt/homebrew/opt/llvm/bin/clang-format) clang-format)
NM ?= $(firstword $(wildcard /opt/homebrew/opt/llvm/bin/llvm-nm) llvm-nm)
OBJCOPY ?= $(firstword $(wildcard /opt/homebrew/opt/llvm/bin/llvm-objcopy) llvm-objcopy)

TARGET := --target=aarch64-none-elf

# See CLAUDE.md "Toolchain and Build" for why each flag is required.
CFLAGS := $(TARGET) -std=c11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
          -mgeneral-regs-only -mstrict-align -O2 -g \
          -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
          -Wstrict-prototypes -Wmissing-prototypes -Wcast-align -Wpointer-arith -Wvla -Wundef \
          -Iinclude -MMD -MP
ASFLAGS := $(TARGET) -Iinclude -MMD -MP
LDFLAGS := -nostdlib -static -z max-page-size=4096 -T link.ld

C_SRCS := $(wildcard kernel/*.c drivers/*.c arch/$(ARCH)/*.c)
ifneq ($(SELFTEST),)
CFLAGS += -DANVIL_SELFTEST -DANVIL_SELFTEST_$(SELFTEST) -Itests/kernel
C_SRCS += tests/kernel/selftest.c
endif
ASM_SRCS := $(wildcard arch/$(ARCH)/*.S)
OBJS := $(patsubst %,$(BUILD)/%.o,$(ASM_SRCS) $(C_SRCS))

QEMU_FLAGS := -M virt -cpu cortex-a72 -m 128M -nographic -no-reboot
# Kill QEMU if the kernel hangs instead of powering off (macOS has no `timeout`).
CHECK_TIMEOUT := perl -e 'alarm shift; exec @ARGV' 10

# Reference manuals (see manuals/README.md). Downloaded, not committed: Arm's license does not
# allow redistributing them.
ARM_DOCS := https://documentation-service.arm.com/static
MANUALS := \
    manuals/DDI0487M_d_arm_architecture_reference_manual.pdf=$(ARM_DOCS)/6abbec2a6db85c5e6614a4a1 \
    manuals/DDI0183G_pl011_uart_trm.pdf=$(ARM_DOCS)/5e8e36c2fd977155116a90b5 \
    manuals/DEN0022F_b_psci.pdf=$(ARM_DOCS)/6703a8b8d7e4b739d817e10d \
    manuals/aapcs64_2026Q3.pdf=https://github.com/ARM-software/abi-aa/releases/download/2026Q3/aapcs64.pdf

.PHONY: all run run-el2 debug check test format format-check clean help manuals

all: $(IMAGE)

$(KERNEL): $(OBJS) link.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(IMAGE): $(KERNEL)
	$(OBJCOPY) -O binary $< $@

$(BUILD)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.S.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

run: $(IMAGE)
	$(QEMU) $(QEMU_FLAGS) -kernel $(IMAGE)

# Same, but QEMU starts the CPU at EL2 (like real hardware), to exercise the EL2 -> EL1 drop.
run-el2: $(IMAGE)
	$(QEMU) $(subst -M virt,-M virt$(comma)virtualization=on,$(QEMU_FLAGS)) -kernel $(IMAGE)

# Freeze the CPU at the first instruction and wait for a debugger on port 1234.
debug: $(IMAGE)
	@echo "In another terminal: lldb $(KERNEL) -o 'gdb-remote 1234'"
	$(QEMU) $(QEMU_FLAGS) -kernel $(IMAGE) -s -S

# Boot one kernel and require that QEMU exits by itself (exit 0, the kernel powered off) and
# that the serial output contains a line matching a pattern.
# $(1) machine, $(2) kernel file, $(3) grep pattern, $(4) test name.
define qemu_expect
	out=$$($(CHECK_TIMEOUT) $(QEMU) $(subst -M virt,-M $(1),$(QEMU_FLAGS)) -kernel $(2) \
	       </dev/null 2>&1); status=$$?; \
	if [ $$status -eq 0 ] && printf '%s\n' "$$out" | tr -d '\r' | grep -q '$(3)'; then \
	    echo "PASS  $(4)"; \
	else \
	    echo "FAIL  $(4) (exit $$status)"; printf '%s\n' "$$out"; exit 1; \
	fi
endef

# Boot an exception self-test kernel and require: the expected exception line, an ELR equal
# to the address of the `selftest_fault` label in that kernel (proof that the trap frame holds
# the real saved state), the register pattern REGS_<name> if one is set, the final panic, and
# QEMU exiting by itself.
# $(1) self-test name, $(2) test name.
define qemu_expect_fault
	kernel=build/selftest-$(1)/anvil.elf; \
	addr=$$($(NM) $$kernel | awk '$$3 == "selftest_fault" { print $$1 }'); \
	out=$$($(CHECK_TIMEOUT) $(QEMU) $(QEMU_FLAGS) -kernel build/selftest-$(1)/anvil.bin </dev/null 2>&1); status=$$?; \
	out=$$(printf '%s\n' "$$out" | tr -d '\r'); \
	if [ -n "$$addr" ] && [ $$status -eq 0 ] \
	   && printf '%s\n' "$$out" | grep -q '$(EXPECT_$(1))' \
	   && printf '%s\n' "$$out" | grep -q "ELR  $$addr" \
	   && printf '%s\n' "$$out" | grep -q '$(REGS_$(1))' \
	   && printf '%s\n' "$$out" | grep -q '^PANIC: unhandled exception$$'; then \
	    echo "PASS  $(2)"; \
	else \
	    echo "FAIL  $(2) (exit $$status, selftest_fault at $$addr)"; printf '%s\n' "$$out"; exit 1; \
	fi
endef

# Expected line of each self-test. Kept in variables because a "(" inside $(call ...) would be
# read as the start of another make function.
EXPECT_ASSERT := ^PANIC: assertion failed: answer == 5 (tests/kernel/selftest.c:[0-9]*)$$
EXPECT_BRK := ^EXCEPTION: BRK (breakpoint instruction) (entry 4)$$
EXPECT_UDF := ^EXCEPTION: unknown reason (undefined instruction?) (entry 4)$$
EXPECT_UNALIGNED := ^EXCEPTION: data abort in kernel (entry 4)$$
# The BRK self-test sets x9 = 0x1234 right before the fault.
REGS_BRK := x9  0000000000001234
FAULT_SELFTESTS := BRK UDF UNALIGNED

# Milestone test: normal boot at EL1 and EL2 entry, then each self-test kernel.
check: $(IMAGE)
	@$(call qemu_expect,virt,$(IMAGE),^Anvil,boot at EL1)
	@$(call qemu_expect,virt$(comma)virtualization=on,$(IMAGE),^Anvil,boot at EL2)
	@$(MAKE) --no-print-directory -s SELFTEST=ASSERT all
	@$(call qemu_expect,virt,build/selftest-ASSERT/anvil.bin,$(EXPECT_ASSERT),KASSERT panics)
	@for t in $(FAULT_SELFTESTS); do $(MAKE) --no-print-directory -s SELFTEST=$$t all || exit 1; done
	@$(call qemu_expect_fault,BRK,brk is caught by the exception handler)
	@$(call qemu_expect_fault,UDF,undefined instruction is caught)
	@$(call qemu_expect_fault,UNALIGNED,unaligned load is caught as a data abort)

# Host unit tests for pure logic: compiled for this Mac (not the kernel), with sanitizers that
# abort on out-of-bounds memory access and undefined behavior.
HOST_CC := cc
HOST_CFLAGS := -std=c11 -g -O1 -Wall -Wextra -Werror -Wshadow -Wconversion -Wsign-conversion \
               -Wstrict-prototypes -Wmissing-prototypes -Wvla -Iinclude \
               -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
HOST_TESTS := $(BUILD)/host/format_test $(BUILD)/host/fdt_test

$(BUILD)/host/format_test: tests/format_test.c kernel/format.c include/anvil/format.h
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) tests/format_test.c kernel/format.c -o $@

$(BUILD)/host/fdt_test: tests/fdt_test.c kernel/fdt.c include/anvil/fdt.h include/anvil/kerr.h
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) tests/fdt_test.c kernel/fdt.c -o $@

test: $(HOST_TESTS)
	@for t in $(HOST_TESTS); do $$t || exit 1; done

format:
	$(CLANG_FORMAT) -i $(C_SRCS) $(wildcard tests/*.c tests/kernel/*.c tests/kernel/*.h include/*/*.h arch/$(ARCH)/*.h)

format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(C_SRCS) $(wildcard tests/*.c tests/kernel/*.c tests/kernel/*.h include/*/*.h arch/$(ARCH)/*.h)

clean:
	rm -rf $(BUILD)

# Download any manual that is missing. Existing files are left alone.
manuals:
	@for entry in $(MANUALS); do \
	    file=$${entry%%=*}; url=$${entry#*=}; \
	    if [ -f "$$file" ]; then echo "have     $$file"; continue; fi; \
	    echo "download $$file"; \
	    curl -fL --retry 2 -o "$$file.part" "$$url" && mv "$$file.part" "$$file" || exit 1; \
	done

help:
	@echo "make           build $(IMAGE) (and $(KERNEL) for lldb)"
	@echo "make run       boot it in QEMU (serial output in this terminal)"
	@echo "make run SELFTEST=BRK|UDF|UNALIGNED|ASSERT"
	@echo "               boot a crash-on-purpose kernel and see its exception dump / panic"
	@echo "make run-el2   boot it with the CPU starting at EL2"
	@echo "make debug     boot frozen, waiting for lldb on port 1234"
	@echo "make check     automatic boot tests (EL1 and EL2 entry, panic self-tests)"
	@echo "make test      host unit tests"
	@echo "make manuals   download the hardware manuals into manuals/"
	@echo "make clean     delete build output"

comma := ,

-include $(OBJS:.o=.d)
