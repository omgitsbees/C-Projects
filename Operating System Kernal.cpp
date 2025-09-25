// boot/boot.asm - Basic bootloader with multiboot support
; Multiboot header
MBALIGN  equ  1 << 0
MEMINFO  equ  1 << 1
FLAGS    equ  MBALIGN | MEMINFO
MAGIC    equ  0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
resb 16384 ; 16 KiB
stack_top:

section .text
global _start:function (_start.end - _start)
_start:
    mov esp, stack_top
    
    ; Call kernel main
    extern kernel_main
    call kernel_main
    
    ; Hang if kernel returns
    cli
.hang: hlt
    jmp .hang
.end:

// ==================================================
// include/types.h - Basic type definitions
#ifndef TYPES_H
#define TYPES_H

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

typedef uint32_t size_t;
typedef uint32_t uintptr_t;

#define NULL ((void*)0)

#endif

// ==================================================
// include/vga.h - VGA text mode driver
#ifndef VGA_H
#define VGA_H

#include "types.h"

enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

class VGATerminal {
private:
    size_t row;
    size_t column;
    uint8_t color;
    uint16_t* buffer;
    
    static const size_t VGA_WIDTH = 80;
    static const size_t VGA_HEIGHT = 25;
    
public:
    VGATerminal();
    void clear();
    void setcolor(uint8_t color);
    void putchar(char c);
    void write(const char* data, size_t size);
    void writestring(const char* data);
    void printf(const char* format, ...);
};

extern VGATerminal terminal;

#endif

// ==================================================
// drivers/vga.cpp - VGA implementation
#include "../include/vga.h"

VGATerminal terminal;

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

VGATerminal::VGATerminal() {
    row = 0;
    column = 0;
    color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    buffer = (uint16_t*) 0xB8000;
    clear();
}

void VGATerminal::clear() {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            buffer[index] = vga_entry(' ', color);
        }
    }
}

void VGATerminal::setcolor(uint8_t color) {
    this->color = color;
}

void VGATerminal::putchar(char c) {
    if (c == '\n') {
        column = 0;
        if (++row == VGA_HEIGHT) {
            // Simple scroll - move all lines up
            for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
                for (size_t x = 0; x < VGA_WIDTH; x++) {
                    buffer[y * VGA_WIDTH + x] = buffer[(y + 1) * VGA_WIDTH + x];
                }
            }
            // Clear last line
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', color);
            }
            row = VGA_HEIGHT - 1;
        }
        return;
    }
    
    const size_t index = row * VGA_WIDTH + column;
    buffer[index] = vga_entry(c, color);
    
    if (++column == VGA_WIDTH) {
        column = 0;
        if (++row == VGA_HEIGHT) {
            row = 0; // Simple wrap for now
        }
    }
}

void VGATerminal::write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++)
        putchar(data[i]);
}

void VGATerminal::writestring(const char* data) {
    size_t len = 0;
    while (data[len]) len++; // Simple strlen
    write(data, len);
}

// ==================================================
// include/memory.h - Memory management
#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

#define PAGE_SIZE 4096
#define KERNEL_BASE 0xC0000000

class PhysicalMemoryManager {
private:
    uint32_t* bitmap;
    uint32_t total_frames;
    uint32_t used_frames;
    
public:
    void initialize(uint32_t mem_size);
    uint32_t allocate_frame();
    void free_frame(uint32_t frame);
    uint32_t get_free_frames() { return total_frames - used_frames; }
};

class VirtualMemoryManager {
private:
    uint32_t* page_directory;
    
public:
    void initialize();
    void map_page(uint32_t virt, uint32_t phys, uint32_t flags);
    void unmap_page(uint32_t virt);
    uint32_t get_physical_address(uint32_t virt);
    void switch_page_directory(uint32_t* pd);
};

extern PhysicalMemoryManager pmm;
extern VirtualMemoryManager vmm;

#endif

// ==================================================
// include/process.h - Process management
#ifndef PROCESS_H
#define PROCESS_H

#include "types.h"

enum ProcessState {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_TERMINATED
};

struct Process {
    uint32_t pid;
    ProcessState state;
    
    // CPU context
    uint32_t esp, ebp;
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi;
    uint32_t eflags;
    
    // Memory management
    uint32_t* page_directory;
    uint32_t kernel_stack;
    
    // Scheduling
    uint32_t time_slice;
    Process* next;
};

class Scheduler {
private:
    Process* current_process;
    Process* ready_queue;
    uint32_t next_pid;
    
public:
    void initialize();
    uint32_t create_process(void (*entry_point)());
    void schedule();
    void yield();
    Process* get_current_process() { return current_process; }
};

extern Scheduler scheduler;

#endif

// ==================================================
// kernel/main.cpp - Kernel entry point
#include "../include/vga.h"
#include "../include/memory.h"
#include "../include/process.h"

// Global objects
PhysicalMemoryManager pmm;
VirtualMemoryManager vmm;
Scheduler scheduler;

extern "C" void kernel_main() {
    // Initialize terminal
    terminal.clear();
    terminal.writestring("MinimalOS Kernel v0.1\n");
    terminal.writestring("Initializing...\n");
    
    // Initialize memory management
    terminal.writestring("Setting up memory management...\n");
    pmm.initialize(32 * 1024 * 1024); // Assume 32MB RAM for now
    vmm.initialize();
    
    // Initialize process scheduler
    terminal.writestring("Setting up process scheduler...\n");
    scheduler.initialize();
    
    // Create first process
    scheduler.create_process([]() {
        for (int i = 0; i < 10; i++) {
            terminal.writestring("Process 1 running\n");
            // Simple delay loop
            for (volatile int j = 0; j < 1000000; j++);
            scheduler.yield();
        }
    });
    
    terminal.writestring("Kernel initialization complete!\n");
    terminal.writestring("Starting scheduler...\n");
    
    // Start scheduling
    scheduler.schedule();
    
    // Should never reach here
    terminal.writestring("Kernel panic: Scheduler returned!\n");
    while (1) {
        asm volatile("hlt");
    }
}

// ==================================================
// Makefile
# Makefile for MinimalOS Kernel

CC = i686-elf-gcc
CXX = i686-elf-g++
AS = nasm
LD = i686-elf-ld

CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra
CXXFLAGS = -std=c++17 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti
ASFLAGS = -felf32
LDFLAGS = -T linker.ld -ffreestanding -O2 -nostdlib

OBJECTS = boot/boot.o kernel/main.o drivers/vga.o

all: kernel.iso

boot/boot.o: boot/boot.asm
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.elf: $(OBJECTS) linker.ld
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) -lgcc

kernel.iso: kernel.elf
	mkdir -p isodir/boot/grub
	cp kernel.elf isodir/boot/kernel.elf
	echo 'menuentry "MinimalOS" {' > isodir/boot/grub/grub.cfg
	echo '    multiboot /boot/kernel.elf' >> isodir/boot/grub/grub.cfg
	echo '}' >> isodir/boot/grub/grub.cfg
	grub-mkrescue -o $@ isodir

run: kernel.iso
	qemu-system-i386 -cdrom kernel.iso

debug: kernel.iso
	qemu-system-i386 -s -S -cdrom kernel.iso &
	gdb kernel.elf -ex "target remote localhost:1234"

clean:
	rm -f $(OBJECTS) kernel.elf kernel.iso
	rm -rf isodir

.PHONY: all run debug clean