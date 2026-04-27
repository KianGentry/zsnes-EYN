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

    while (total_read < size) {
        ssize_t chunk = read(fd, buffer + total_read, size - total_read);
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

    /* Try to detect from header signature */
    uint32_t offset = (size >= 0x10000) ? HEADER_OFFSET_HIROM : HEADER_OFFSET_LOROM;
    if (offset < size) {
        if (data[offset + HEADER_MAGIC_OFFSET] == HEADER_MAGIC_VALUE) {
            /* Signature present; likely valid header */
            return ROM_FORMAT_SFC;  /* Linear format */
        }
    }

    /* Fall back to file extension */
    return rom_detect_from_extension(path);
}

int rom_parse_header(zsnes_emu_t *emu) {
    if (!emu || !emu->rom.data)
        return -1;

    /* Determine header offset based on ROM size and format */
    uint32_t offset;
    if (emu->rom.size >= 0x10000 && emu->rom.format == ROM_FORMAT_SFC) {
        offset = HEADER_OFFSET_HIROM;
    } else {
        offset = HEADER_OFFSET_LOROM;
    }

    /* Ensure offset is within ROM bounds */
    if (offset + 32 > emu->rom.size) {
        fprintf(stderr, "Warning: ROM too small for header at offset 0x%X\n", offset);
        return -1;
    }

    uint8_t *header = &emu->rom.data[offset];

    /* Extract title (21 bytes, zero-padded) */
    memcpy(emu->rom.title, header + HEADER_TITLE_OFFSET, HEADER_TITLE_LEN);
    emu->rom.title[HEADER_TITLE_LEN] = '\0';

    /* Extract cartridge type and size info */
    emu->rom.map_mode = header[HEADER_MAPMODE_OFFSET];
    emu->rom.cartridge_type = header[HEADER_TYPE_OFFSET];
    emu->rom.rom_size_flag = header[HEADER_ROMSIZE_OFFSET];
    emu->rom.ram_size_flag = header[HEADER_RAMSIZE_OFFSET];

    printf("ROM Loaded: %s\n", emu->rom.title);
    printf("Map mode: 0x%02X, Cartridge type: 0x%02X\n", emu->rom.map_mode, emu->rom.cartridge_type);

    return 0;
}

int rom_deinterleave(uint8_t *data, size_t size) {
    /* TODO: Implement SMC deinterleaving
     *
     * SMC format interleaves 16 KB blocks in a specific pattern.
     * This function should de-interleave the data to produce linear ROM.
     */
    (void)data;
    (void)size;
    return 0;
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

    /* De-interleave if necessary */
    if (emu->rom.format == ROM_FORMAT_SMC) {
        if (rom_deinterleave(buffer, size) < 0) {
            fprintf(stderr, "Error: Failed to de-interleave SMC ROM\n");
            free(buffer);
            return -1;
        }
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
