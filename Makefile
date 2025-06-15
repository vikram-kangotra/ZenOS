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

TARGET := $(DIST_DIR)/x86_64/kernel.elf
ISO := $(DIST_DIR)/x86_64/kernel.iso
DISK_IMG := $(DIST_DIR)/x86_64/disk.img

AS := nasm
CC := gcc
LD := ld
GRUB_MKRESCUE := grub-mkrescue

ASMFLAGS := -f elf64 -I $(ASM_DIR)
CFLAGS := -O3 -Iinclude -m64 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -mno-red-zone -mgeneral-regs-only
LDFLAGS := -T targets/x86_64/linker.ld -melf_x86_64

ASM_SRC := $(shell find $(ASM_DIR) $(COMMON_ARCH_DIR) -name '*.asm')
C_SRC := $(shell find $(C_DIR) $(KERNEL_C_DIR) $(FS_C_DIR) $(MULTIBOOT_C_DIR) $(COMMON_ARCH_DIR) -name '*.c')

# WebAssembly source files
WASM_SRC = src/wasm/src/wasm.c \
           src/wasm/src/wasm_parser.c \
           src/wasm/src/wasm_exec.c \
           src/wasm/src/wasm_kernel.c

# Add WebAssembly source files to C source files
C_SRC += $(WASM_SRC)

OBJ := $(ASM_SRC:$(SRC_DIR)/%.asm=$(BUILD_DIR)/%.o) $(C_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# WebAssembly tools
WAT2WASM := wat2wasm

# WebAssembly test module
WASM_TEST := $(DIST_DIR)/x86_64/test.wasm

# C++ to WebAssembly tools
CXX := g++
WASM_CXX := em++
WASM_FLAGS := -O3 -s WASM=1 -s EXPORTED_FUNCTIONS='["_main"]' -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' -s ALLOW_MEMORY_GROWTH=1 -s STANDALONE_WASM=1

# C++ program directory
CPP_DIR := programs/cpp
WASM_PROGRAMS_DIR := $(DIST_DIR)/x86_64/wasm_programs

# Find all C++ source files
CPP_SRC := $(shell find $(CPP_DIR) -name '*.cpp')
WASM_BINS := $(CPP_SRC:$(CPP_DIR)/%.cpp=$(WASM_PROGRAMS_DIR)/%.wasm)

# Emscripten SDK setup
EMSDK_DIR := emsdk

# Add submodule initialization target
.PHONY: init-submodules
init-submodules:
	git submodule update --init --recursive
	cd $(EMSDK_DIR) && ./emsdk install latest && ./emsdk activate latest

$(ISO): $(TARGET) $(ISO_DIR)/boot/grub/grub.cfg
	@mkdir -p $(ISO_DIR)/boot
	cp $(TARGET) $(ISO_DIR)/boot/kernel.elf
	$(GRUB_MKRESCUE) -o $(ISO) $(ISO_DIR)

$(TARGET): $(OBJ)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(@D)
	$(AS) $(ASMFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Create FAT32 disk image
$(DISK_IMG): $(WASM_TEST) $(WASM_BINS) init-submodules
	@mkdir -p $(@D)
	# Create a 512MB disk to ensure proper FAT32 operation
	dd if=/dev/zero of=$@ bs=1M count=512
	# Format with specific parameters for FAT32
	mkfs.fat -F 32 -S 512 -s 4 -R 32 -f 2 -n "ZENOS" $@
	# Verify filesystem
	fsck.fat -v $@
	# Create directories and verify
	mmd -i $@ ::/dev ::/proc ::/bin ::/etc ::/lib ::/mnt ::/opt ::/root ::/tmp ::/wasm
	# Copy Readme.md to disk
	mcopy -i $@ Readme.md ::/Readme.md
	# Copy WebAssembly test module
	mcopy -i $@ $(WASM_TEST) ::/test.wsm
	# Copy WebAssembly programs
	mcopy -i $@ $(WASM_PROGRAMS_DIR)/*.wasm ::/wasm/
	# List directory contents to verify
	mdir -i $@ ::
	# Print filesystem info
	fsck.fat -v $@

# Compile WebAssembly test module
$(WASM_TEST): src/wasm/test/test.wat
	@mkdir -p $(@D)
	@if ! command -v $(WAT2WASM) >/dev/null 2>&1; then \
		echo "Error: wat2wasm not found. Please install wabt (WebAssembly Binary Toolkit) to build test.wasm."; \
		exit 1; \
	fi
	$(WAT2WASM) $< -o $@

# Compile C++ to WebAssembly
$(WASM_PROGRAMS_DIR)/%.wasm: $(CPP_DIR)/%.cpp emsdk
	@mkdir -p $(@D)
	@cd $(EMSDK_DIR) && . ./emsdk_env.sh && cd .. && \
	$(WASM_CXX) $(WASM_FLAGS) $< -o $@

.PHONY: clean run disk verify-disk
clean:
	rm -rf $(BUILD_DIR) $(DIST_DIR)
	rm -f $(ISO_DIR)/boot/kernel.elf
	git submodule deinit -f --all

disk: $(DISK_IMG)

# Add a target to verify the disk image
verify-disk: $(DISK_IMG)
	@echo "Verifying disk image..."
	fsck.fat -v $<
	mdir -i $< ::

run: $(ISO) $(DISK_IMG)
	qemu-system-x86_64 \
		-cdrom $(ISO) \
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

# Add a target to update submodules
.PHONY: update-submodules
update-submodules:
	git submodule update --remote --merge
