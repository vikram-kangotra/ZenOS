# ZenOS

ZenOS is an Operating System with native WebAssembly support.

## Pre-requisite

- ld
- gcc
- nasm
- make
- grub-mkrescue
- qemu-system-x86_64
- bochs

## Compile

```sh
make
```

## Run

```sh
qemu-system-x86_64 -cdrom dist/x86_64/kernel.iso
```

## Debug

```sh
bochs -f bochsrc.txt -q
```
