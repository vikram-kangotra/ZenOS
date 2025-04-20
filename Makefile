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

OBJ := $(ASM_SRC:$(SRC_DIR)/%.asm=$(BUILD_DIR)/%.o) $(C_SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

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
$(DISK_IMG):
	@mkdir -p $(@D)
	# Create a larger disk (128MB) to ensure proper FAT32 operation
	qemu-img create -f raw $@ 128M
	# Format with specific cluster size (4KB) and verify
	mkfs.fat -F 32 -n "ZENOS" -s 8 -S 512 $@
	# Verify filesystem
	fsck.fat -n $@
	# Create directories and verify
	mmd -i $@ ::/dev ::/proc ::/bin ::/etc ::/lib ::/mnt ::/opt ::/root ::/run ::/sbin ::/srv ::/tmp ::/usr ::/var
	# List directory contents to verify
	mdir -i $@ ::
	# Print filesystem info
	fsck.fat -v $@

.PHONY: clean run disk verify-disk
clean:
	rm -rf $(BUILD_DIR) $(DIST_DIR)
	rm -f $(ISO_DIR)/boot/kernel.elf

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
