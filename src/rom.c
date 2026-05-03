/*
 * SNES ROM Loading and Format Detection
 *
 * Handles loading SNES ROM files in both SMC (interleaved) and SFC (linear) formats,
 * parsing cartridge headers, and extracting ROM metadata.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "rom.h"

/* SNES cartridge header offsets (in linear ROM at 0x7FB0 or 0xFFB0 depending on mode) */
#define HEADER_OFFSET_LINEAR    0xFFB0
#define HEADER_OFFSET_LOROM     0x7FB0
#define HEADER_OFFSET_HIROM     0xFFB0

#define HEADER_TITLE_OFFSET     0x00
#define HEADER_TITLE_LEN        21
#define HEADER_MAPMODE_OFFSET   0x15
#define HEADER_TYPE_OFFSET      0x16
#define HEADER_ROMSIZE_OFFSET   0x17
#define HEADER_RAMSIZE_OFFSET   0x18

/* Magic signature check */
#define HEADER_MAGIC_OFFSET     0x1D
#define HEADER_MAGIC_VALUE      0x33

static size_t rom_detect_copier_header_size(size_t size) {
    if (size > 512 && ((size - 512) % 0x8000) == 0) {
        return 512;
    }

    return 0;
}

static int rom_header_is_plausible(const uint8_t *header) {
    if (!header) {
        return 0;
    }

    return header[HEADER_MAGIC_OFFSET] == HEADER_MAGIC_VALUE;
}

static rom_format_t rom_detect_from_extension(const char *path) {
    if (!path)
        return ROM_FORMAT_UNKNOWN;

    const char *ext = strrchr(path, '.');
    if (!ext)
        return ROM_FORMAT_UNKNOWN;

    if (strcmp(ext, ".sfc") == 0) {
        return ROM_FORMAT_SFC;
    } else if (strcmp(ext, ".smc") == 0) {
        return ROM_FORMAT_SMC;
    }

    return ROM_FORMAT_UNKNOWN;
}

static ssize_t rom_read_full(int fd, uint8_t *buffer, size_t size) {
    size_t total_read = 0;
    size_t chunk_size = 32 * 1024;  /* Read in 32KB chunks to work around syscall limits */

    while (total_read < size) {
        size_t to_read = (size - total_read < chunk_size) ? (size - total_read) : chunk_size;
        ssize_t chunk = read(fd, buffer + total_read, to_read);
        if (chunk < 0) {
            return -1;
        }
        if (chunk == 0) {
            break;
        }

        total_read += (size_t)chunk;
    }

    return (ssize_t)total_read;
}

rom_format_t rom_detect_format(const uint8_t *data, size_t size, const char *path) {
    if (!data || size < 0x8000)
        return ROM_FORMAT_UNKNOWN;

    size_t header_size = rom_detect_copier_header_size(size);

    /* Try to detect from header signature, preferring LoROM because it is
     * the common layout for the included test ROMs and many commercial carts.
     */
    if (header_size + HEADER_OFFSET_LOROM + HEADER_MAGIC_OFFSET < size &&
        data[header_size + HEADER_OFFSET_LOROM + HEADER_MAGIC_OFFSET] == HEADER_MAGIC_VALUE) {
        return ROM_FORMAT_SFC;
    }

    if (header_size + HEADER_OFFSET_HIROM + HEADER_MAGIC_OFFSET < size &&
        data[header_size + HEADER_OFFSET_HIROM + HEADER_MAGIC_OFFSET] == HEADER_MAGIC_VALUE) {
        return ROM_FORMAT_SFC;
    }

    /* Fall back to file extension */
    return rom_detect_from_extension(path);
}

int rom_parse_header(zsnes_emu_t *emu) {
    if (!emu || !emu->rom.data)
        return -1;

    /* Determine header offset by probing both common layouts. */
    size_t header_size = emu->rom.header_size;
    const uint8_t *header = NULL;
    int is_hirom = 0;

    if (header_size + HEADER_OFFSET_LOROM + 32 <= emu->rom.size) {
        header = &emu->rom.data[header_size + HEADER_OFFSET_LOROM];
        if (!rom_header_is_plausible(header) &&
            header_size + HEADER_OFFSET_HIROM + 32 <= emu->rom.size) {
            header = NULL;
        }
    }

    if (!header && header_size + HEADER_OFFSET_HIROM + 32 <= emu->rom.size) {
        header = &emu->rom.data[header_size + HEADER_OFFSET_HIROM];
        is_hirom = 1;
    }

    if (!header) {
        fprintf(stderr, "Warning: ROM too small or missing internal header\n");
        return -1;
    }

    /* Extract title (21 bytes, zero-padded) */
    memcpy(emu->rom.title, header + HEADER_TITLE_OFFSET, HEADER_TITLE_LEN);
    emu->rom.title[sizeof(emu->rom.title) - 1] = '\0';

    /* Extract cartridge type and size info */
    emu->rom.map_mode = header[HEADER_MAPMODE_OFFSET];
    emu->rom.cartridge_type = header[HEADER_TYPE_OFFSET];
    emu->rom.rom_size_flag = header[HEADER_ROMSIZE_OFFSET];
    emu->rom.ram_size_flag = header[HEADER_RAMSIZE_OFFSET];
    emu->rom.is_hirom = (uint8_t)is_hirom;

    /* Reset vector is stored near the end of the mapped ROM area. */
    if (header_size + (is_hirom ? HEADER_OFFSET_HIROM : HEADER_OFFSET_LOROM) + 0x7FFE < emu->rom.size) {
        uint32_t vector_base = header_size + (is_hirom ? HEADER_OFFSET_HIROM : HEADER_OFFSET_LOROM);
        emu->rom.reset_vector = (uint16_t)(emu->rom.data[vector_base + 0x7FFC - (is_hirom ? HEADER_OFFSET_HIROM : HEADER_OFFSET_LOROM)] |
                                           (emu->rom.data[vector_base + 0x7FFD - (is_hirom ? HEADER_OFFSET_HIROM : HEADER_OFFSET_LOROM)] << 8));
    } else {
        emu->rom.reset_vector = 0x8000;
    }

    printf("ROM Loaded: %s\n", emu->rom.title);
    printf("Map mode: 0x%02X, Cartridge type: 0x%02X, layout: %s, reset: 0x%04X\n",
           emu->rom.map_mode,
           emu->rom.cartridge_type,
           is_hirom ? "HiROM" : "LoROM",
           emu->rom.reset_vector);

    return 0;
}

size_t rom_deinterleave(uint8_t *data, size_t size) {
    /* Typical SMC images store a 512-byte copier header, followed by
     * 32 KiB chunks whose 16 KiB halves are swapped. Normalize that into
     * linear ROM layout in-place.
     */
    if (!data || size < 0x8000) {
        return 0;
    }

    size_t header_size = 0;
    if (size > 512 && ((size - 512) % 0x8000) == 0) {
        header_size = 512;
    } else if ((size % 0x8000) != 0) {
        return 0;
    }

    size_t payload_size = size - header_size;
    size_t block_count = payload_size / 0x8000;
    uint8_t block[0x8000];

    for (size_t block_index = 0; block_index < block_count; ++block_index) {
        uint8_t *src = data + header_size + (block_index * 0x8000);
        uint8_t *dst = data + (block_index * 0x8000);

        memcpy(block, src, sizeof(block));
        memcpy(dst, block + 0x4000, 0x4000);
        memcpy(dst + 0x4000, block, 0x4000);
    }

    return payload_size;
}

int rom_load(zsnes_emu_t *emu, const char *path) {
    if (!emu || !path)
        return -1;

    /* Open ROM file */
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error: Failed to open ROM file\n");
        return -1;
    }

    /* Get file size */
    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0 || size > SNES_ROM_MAX) {
        fprintf(stderr, "Error: ROM size %lu exceeds maximum %u bytes\n", (unsigned long)size, SNES_ROM_MAX);
        close(fd);
        return -1;
    }

    lseek(fd, 0, SEEK_SET);

    /* Allocate ROM buffer */
    uint8_t *buffer = (uint8_t *)malloc(size);
    if (!buffer) {
        fprintf(stderr, "Error: Failed to allocate ROM buffer (%lu bytes)\n", (unsigned long)size);
        close(fd);
        return -1;
    }

    /* Read ROM data */
    ssize_t bytes_read = rom_read_full(fd, buffer, (size_t)size);
    close(fd);

    if (bytes_read != size) {
        fprintf(stderr, "Error: Failed to read complete ROM (read %ld of %lu bytes)\n", (long)bytes_read, (unsigned long)size);
        free(buffer);
        return -1;
    }

    /* Detect format */
    emu->rom.format = rom_detect_format(buffer, size, path);
    if (emu->rom.format == ROM_FORMAT_UNKNOWN) {
        fprintf(stderr, "Error: Unknown ROM format. Expected .sfc or .smc\n");
        free(buffer);
        return -1;
    }

    emu->rom.header_size = rom_detect_copier_header_size((size_t)size);
    emu->rom.reset_vector = 0x8000;
    emu->rom.is_hirom = 0;

    /* De-interleave if necessary */
    if (emu->rom.format == ROM_FORMAT_SMC) {
        size_t new_size = rom_deinterleave(buffer, (size_t)size);
        if (new_size == 0) {
            fprintf(stderr, "Error: Failed to de-interleave SMC ROM\n");
            free(buffer);
            return -1;
        }
        size = (off_t)new_size;
        emu->rom.header_size = 0;
    }

    /* Store ROM data and metadata */
    emu->rom.data = buffer;
    emu->rom.size = size;

    /* Parse header */
    if (rom_parse_header(emu) < 0) {
        fprintf(stderr, "Warning: Failed to parse ROM header; continuing with defaults\n");
    }

    return 0;
}

void rom_unload(zsnes_emu_t *emu) {
    if (emu && emu->rom.data) {
        free(emu->rom.data);
        emu->rom.data = NULL;
        emu->rom.size = 0;
    }
}
