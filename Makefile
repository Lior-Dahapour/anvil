# Anvil build. Run `make help` for the list of targets.

ARCH ?= aarch64
BUILD := build
KERNEL := $(BUILD)/anvil.elf

CC := clang
LD := ld.lld
QEMU := qemu-system-$(ARCH)
CLANG_FORMAT ?= $(firstword $(wildcard /opt/homebrew/opt/llvm/bin/clang-format) clang-format)

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

all: $(KERNEL)

$(KERNEL): $(OBJS) link.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.S.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

run: $(KERNEL)
	$(QEMU) $(QEMU_FLAGS) -kernel $(KERNEL)

# Same, but QEMU starts the CPU at EL2 (like real hardware), to exercise the EL2 -> EL1 drop.
run-el2: $(KERNEL)
	$(QEMU) $(subst -M virt,-M virt$(comma)virtualization=on,$(QEMU_FLAGS)) -kernel $(KERNEL)

# Freeze the CPU at the first instruction and wait for a debugger on port 1234.
debug: $(KERNEL)
	@echo "In another terminal: lldb $(KERNEL) -o 'gdb-remote 1234'"
	$(QEMU) $(QEMU_FLAGS) -kernel $(KERNEL) -s -S

# Milestone test: must print "Anvil" and power off by itself, at both EL1 and EL2 entry.
check: $(KERNEL)
	@for machine in virt virt,virtualization=on; do \
	    out=$$($(CHECK_TIMEOUT) $(QEMU) $(subst -M virt,-M $$machine,$(QEMU_FLAGS)) \
	           -kernel $(KERNEL) </dev/null 2>&1); status=$$?; \
	    if [ $$status -eq 0 ] && printf '%s\n' "$$out" | grep -q '^Anvil'; then \
	        echo "PASS  $$machine"; \
	    else \
	        echo "FAIL  $$machine (exit $$status)"; printf '%s\n' "$$out"; exit 1; \
	    fi; \
	done

# Host unit tests for pure logic. Milestone 1 has none yet.
test:
	@echo "no host tests yet"

format:
	$(CLANG_FORMAT) -i $(C_SRCS) $(wildcard include/*/*.h arch/$(ARCH)/*.h)

format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(C_SRCS) $(wildcard include/*/*.h arch/$(ARCH)/*.h)

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
	@echo "make           build $(KERNEL)"
	@echo "make run       boot it in QEMU (serial output in this terminal)"
	@echo "make run-el2   boot it with the CPU starting at EL2"
	@echo "make debug     boot frozen, waiting for lldb on port 1234"
	@echo "make check     automatic boot test (EL1 and EL2 entry)"
	@echo "make test      host unit tests"
	@echo "make manuals   download the hardware manuals into manuals/"
	@echo "make clean     delete build output"

comma := ,

-include $(OBJS:.o=.d)
