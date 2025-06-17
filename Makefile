# ZenOS Build System
# ==================

# Architecture and Directory Configuration
# --------------------------------------
ARCH := x86_64
SRC_DIR := src
BUILD_DIR := build
DIST_DIR := dist
ARCH_DIR := $(SRC_DIR)/arch
COMMON_ARCH_DIR := $(ARCH_DIR)/common
ASM_DIR := $(ARCH_DIR)/$(ARCH)
C_DIR := $(ARCH_DIR)/$(ARCH)
KERNEL_C_DIR := $(SRC_DIR)/kernel
FS_C_DIR := $(SRC_DIR)/fs
MULTIBOOT_C_DIR := $(SRC_DIR)/multiboot2
ISO_DIR := targets/$(ARCH)/iso

# Output Files
# -----------
TARGET := $(DIST_DIR)/$(ARCH)/kernel.elf
ISO := $(DIST_DIR)/$(ARCH)/kernel.iso
DISK_IMG := $(DIST_DIR)/$(ARCH)/disk.img

# Tool Configuration
# ----------------
AS := nasm
CC := gcc
LD := ld
GRUB_MKRESCUE := grub-mkrescue
WAT2WASM := wat2wasm
CXX := g++
WASM_CXX := em++

# Compiler and Linker Flags
# ----------------------
ASMFLAGS := -f elf64 -I $(ASM_DIR)
CFLAGS := -O3 -Iinclude -m64 -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
          -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -mno-red-zone \
          -mgeneral-regs-only
LDFLAGS := -T targets/$(ARCH)/linker.ld -melf_x86_64

# WebAssembly Configuration
# ----------------------
WASM_FLAGS := -O3 -s WASM=1 \
              -s EXPORTED_FUNCTIONS='["_main"]' \
              -s ALLOW_MEMORY_GROWTH=1 \
              -s STANDALONE_WASM=1 \
              -s NO_FILESYSTEM=1 \
              -s IMPORTED_MEMORY=0 \
              -s ENVIRONMENT='shell' \
              -s ASSERTIONS=0 \
              -s SAFE_HEAP=0 \
              -s STACK_OVERFLOW_CHECK=0 \
              -s DEMANGLE_SUPPORT=0 \
              -s NO_FETCH=1 \
              -s NO_ASYNCIFY=1 \
              -s PURE_WASI=0

# WebAssembly Tools
# --------------
WASM2WAT := wasm2wat

# Directory Configuration
# --------------------
CPP_DIR := programs/cpp
WASM_PROGRAMS_DIR := $(DIST_DIR)/$(ARCH)/wasm_programs
WASM_WAT_DIR := $(DIST_DIR)/$(ARCH)/wasm_wat
EMSDK_DIR := emsdk

# Source Files
# ----------
ASM_SRC := $(shell find $(ASM_DIR) $(COMMON_ARCH_DIR) -name '*.asm')
C_SRC := $(shell find $(C_DIR) $(KERNEL_C_DIR) $(FS_C_DIR) $(MULTIBOOT_C_DIR) \
          $(COMMON_ARCH_DIR) -name '*.c')
WASM_SRC := src/wasm/src/wasm.c \
            src/wasm/src/wasm_parser.c \
            src/wasm/src/wasm_exec.c \
            src/wasm/src/wasm_kernel.c
C_SRC += $(WASM_SRC)

# Object Files
# ----------
OBJ := $(ASM_SRC:$(SRC_DIR)/%.asm=$(BUILD_DIR)/%.o) \
       $(C_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# WebAssembly Files
# --------------
CPP_SRC := $(shell find $(CPP_DIR) -name '*.cpp')
WASM_BINS := $(CPP_SRC:$(CPP_DIR)/%.cpp=$(WASM_PROGRAMS_DIR)/%.wasm)
WASM_WAT_FILES := $(CPP_SRC:$(CPP_DIR)/%.cpp=$(WASM_WAT_DIR)/%.wat)

# Main Targets
# ----------
.PHONY: all
all: check-prerequisites $(TARGET) $(ISO) $(DISK_IMG) wat

.PHONY: kernel
kernel: $(TARGET)

.PHONY: iso
iso: $(ISO)

.PHONY: disk
disk: $(DISK_IMG)

# Prerequisites Check
# ----------------
.PHONY: check-prerequisites
check-prerequisites:
	@echo "Checking build prerequisites..."
	@command -v $(AS) >/dev/null 2>&1 || { echo "Error: $(AS) not found. Please install nasm."; exit 1; }
	@command -v $(CC) >/dev/null 2>&1 || { echo "Error: $(CC) not found. Please install gcc."; exit 1; }
	@command -v $(LD) >/dev/null 2>&1 || { echo "Error: $(LD) not found. Please install binutils."; exit 1; }
	@command -v $(GRUB_MKRESCUE) >/dev/null 2>&1 || { echo "Error: $(GRUB_MKRESCUE) not found. Please install grub."; exit 1; }
	@command -v $(WAT2WASM) >/dev/null 2>&1 || { echo "Error: $(WAT2WASM) not found. Please install wabt."; exit 1; }
	@command -v qemu-system-$(ARCH) >/dev/null 2>&1 || { echo "Error: qemu-system-$(ARCH) not found. Please install qemu."; exit 1; }
	@echo "All prerequisites satisfied."

# Build Rules
# ---------
$(TARGET): $(OBJ)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(@D)
	$(AS) $(ASMFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# ISO Creation
# ----------
$(ISO): $(TARGET) $(ISO_DIR)/boot/grub/grub.cfg
	@mkdir -p $(ISO_DIR)/boot
	cp $(TARGET) $(ISO_DIR)/boot/kernel.elf
	$(GRUB_MKRESCUE) -o $@ $(ISO_DIR)

# Disk Image Creation
# ----------------
$(DISK_IMG): $(WASM_BINS) init-submodules wat
	@mkdir -p $(@D)
	@echo "Creating disk image..."
	dd if=/dev/zero of=$@ bs=1M count=512
	mkfs.fat -F 32 -S 512 -s 4 -R 32 -f 2 -n "ZENOS" $@
	fsck.fat -v $@
	@echo "Creating directory structure..."
	mmd -i $@ ::/dev ::/proc ::/bin ::/etc ::/lib ::/mnt ::/opt ::/root ::/tmp ::/wasm
	@echo "Copying files..."
	mcopy -i $@ Readme.md ::/Readme.md
	mcopy -i $@ $(WASM_PROGRAMS_DIR)/*.wasm ::/wasm/
	@echo "Verifying disk image..."
	mdir -i $@ ::
	fsck.fat -v $@

# WebAssembly Compilation
# --------------------
$(WASM_PROGRAMS_DIR)/%.wasm: $(CPP_DIR)/%.cpp emsdk
	@mkdir -p $(@D)
	@cd $(EMSDK_DIR) && . ./emsdk_env.sh && cd .. && \
	$(WASM_CXX) $(WASM_FLAGS) $< -o $@

$(WASM_WAT_DIR)/%.wat: $(WASM_PROGRAMS_DIR)/%.wasm
	@mkdir -p $(@D)
	$(WASM2WAT) $< -o $@

# Emscripten SDK Management
# ----------------------
.PHONY: init-submodules
init-submodules:
	git submodule update --init --recursive
	cd $(EMSDK_DIR) && ./emsdk install latest && ./emsdk activate latest

.PHONY: update-submodules
update-submodules:
	git submodule update --remote --merge

# QEMU Run Configuration
# -------------------
QEMU_FLAGS := -cdrom $(ISO) \
              -drive file=$(DISK_IMG),format=raw,if=ide,index=0,media=disk \
              -serial file:kernel.log \
              -m 1024 \
              -vga std \
              -device VGA,vgamem_mb=1024 \
              -display sdl \
              -full-screen \
              -boot d \
              -enable-kvm \
              -cpu host

# Run and Debug Targets
# ------------------
.PHONY: run
run: all
	qemu-system-$(ARCH) $(QEMU_FLAGS)

.PHONY: debug
debug: all
	qemu-system-$(ARCH) $(QEMU_FLAGS) -s -S

# Cleanup Targets
# ------------
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR) $(DIST_DIR)
	rm -f $(ISO_DIR)/boot/kernel.elf

.PHONY: distclean
distclean: clean
	@echo "Cleaning all generated files and submodules..."
	git submodule deinit -f --all
	rm -rf $(EMSDK_DIR)

# Help Target
# ---------
.PHONY: help
help:
	@echo "ZenOS Build System"
	@echo "=================="
	@echo "Available targets:"
	@echo "  all          - Build everything (kernel, ISO, and disk image)"
	@echo "  kernel       - Build only the kernel"
	@echo "  iso          - Build only the ISO image"
	@echo "  disk         - Build only the disk image"
	@echo "  run          - Build and run in QEMU"
	@echo "  debug        - Build and run in QEMU with GDB server"
	@echo "  clean        - Remove build artifacts"
	@echo "  distclean    - Remove all generated files and submodules"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "WebAssembly targets:"
	@echo "  emsdk        - Install and activate Emscripten SDK"
	@echo "  init-submodules    - Initialize Git submodules"
	@echo "  update-submodules  - Update Git submodules"
	@echo "  wat          - Generate WAT files from WebAssembly binaries"

# Add wat target
.PHONY: wat
wat: $(WASM_WAT_FILES)
	@echo "Generated WAT files:"
	@ls -l $(WASM_WAT_DIR)/*.wat
	@ls -l $(WASM_TEST_WAT)
