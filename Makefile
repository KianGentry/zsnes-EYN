# ZSNES for EYN-OS - Build configuration
#
# This package is built by the parent EYN-packages build system.
# See EYN-packages/devtools/build_user_c.sh for compilation details.
#
# Manual build (for reference):
#   i686-elf-gcc -m32 -ffreestanding -fno-builtin \
#     -I../../userland/include \
#     -Iinclude \
#     src/rom.c src/cpu.c src/ppu.c src/smp.c zsnes_1_51_uelf.c \
#     -o zsnes_1_51.elf \
#     -T../../devtools/user_elf32.ld \
#     -nostdlib

# This Makefile is mainly for reference and local testing
# The actual build is orchestrated by EYN-packages/devtools/build_user_c.sh

CC ?= i686-elf-gcc
CFLAGS = -m32 -ffreestanding -fno-builtin -fno-pie -fno-pic -fno-plt \
         -fno-stack-protector -nostdlib -nostartfiles -O2 -Wall -Wextra
INCLUDE = -Iinclude -I../../userland/include
LDFLAGS = -T../../devtools/user_elf32.ld -nostdlib

SRC = zsnes_1_51_uelf.c src/rom.c src/cpu.c src/ppu.c src/smp.c
OBJ = $(SRC:.c=.o)
TARGET = zsnes_1_51.elf

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lgcc

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
