# ZenOS

ZenOS is a modern operating system with native WebAssembly support, designed for educational and experimental purposes. It provides a minimal but functional environment for running WebAssembly modules directly on bare metal.

## Features

- Native WebAssembly runtime
- Command-line interface (CLI)
- FAT32 filesystem support
- Memory management with buddy allocator
- ATA disk driver
- VGA text mode display
- Keyboard input support
- Real-time clock (RTC) support
- Programmable Interval Timer (PIT)

## Prerequisites

- `ld` - GNU Linker
- `gcc` - GNU Compiler Collection
- `nasm` - Netwide Assembler
- `make` - GNU Make
- `grub-mkrescue` - GRUB2 rescue image creator
- `qemu-system-x86_64` - QEMU emulator
- `bochs` - x86 PC emulator and debugger

## Building

1. Clone the repository:
```sh
git clone https://github.com/yourusername/ZenOS.git
cd ZenOS
```

2. Build the project:
```sh
make
```

This will create a bootable ISO image in `dist/x86_64/kernel.iso`.

## Running

### Using QEMU (Recommended for quick testing)
```sh
make run
```

### Using Bochs (Recommended for debugging)
```sh
bochs -f bochsrc.txt -q
```

## WebAssembly Support

ZenOS includes a native WebAssembly runtime that supports:
- Loading and parsing WebAssembly modules
- Executing WebAssembly functions
- Basic WebAssembly instructions (i32 operations, control flow, etc.)

### Running WebAssembly Programs

1. Create a WebAssembly module (`.wasm` file)
2. Copy it to the ZenOS filesystem
3. Use the `wasm` command to execute it:
```
> wasmrun /wasm/test~1.was
```

### Built-in WebAssembly Tests

Run the built-in WebAssembly tests using:
```
> wasmtest
```

### Developers
- Vikram Kangotra (vikramkangotra8055@gmail.com) 9419542283
- Disha baghel (bagheldisha708@gmail.com) 9773539142
- Akshita Singh (akshitasingh202@gmail.com) 9334962740
