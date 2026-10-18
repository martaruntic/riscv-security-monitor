CROSS_PREFIX ?= riscv64-unknown-elf-
CC      := $(CROSS_PREFIX)gcc
OBJDUMP := $(CROSS_PREFIX)objdump

BUILD_DIR := build
TARGET    := $(BUILD_DIR)/monitor.elf

CFLAGS := -march=rv64imac_zicsr -mabi=lp64 -mcmodel=medany \
          -ffreestanding -fno-builtin -nostdlib -fno-pie -no-pie \
          -O1 -g -Wall -Wextra -Iinclude
LDFLAGS := -T linker.ld -nostdlib

SOURCES_C := $(wildcard src/*.c)
SOURCES_S := $(wildcard src/*.S)
OBJECTS   := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES_C)) \
             $(patsubst src/%.S,$(BUILD_DIR)/%.o,$(SOURCES_S))

QEMU       := qemu-system-riscv64
QEMU_FLAGS := -machine virt -cpu rv64 -smp 1 -m 128M -nographic -bios none

.PHONY: all run debug dump clean

all: $(TARGET)

$(TARGET): $(OBJECTS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: $(TARGET)
	$(QEMU) $(QEMU_FLAGS) -kernel $(TARGET)

# Starts QEMU halted and waits for gdb on port 1234
debug: $(TARGET)
	$(QEMU) $(QEMU_FLAGS) -kernel $(TARGET) -s -S

dump: $(TARGET)
	$(OBJDUMP) -d $(TARGET) > $(BUILD_DIR)/monitor.asm

clean:
	rm -rf $(BUILD_DIR)
