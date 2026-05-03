/*
 * SNES ROM Loading and Format Detection
 */

#ifndef ROM_H
#define ROM_H

#include "zsnes.h"

/* Load ROM from file path */
int rom_load(zsnes_emu_t *emu, const char *path);

/* Unload ROM and free resources */
void rom_unload(zsnes_emu_t *emu);

/* Detect ROM format from header or file extension */
rom_format_t rom_detect_format(const uint8_t *data, size_t size, const char *path);

/* Parse SNES cartridge header and populate rom_t struct */
int rom_parse_header(zsnes_emu_t *emu);

/* De-interleave SMC format ROM to linear layout.
 * Returns the new payload size after any copier header is removed.
 */
size_t rom_deinterleave(uint8_t *data, size_t size);

#endif /* ROM_H */
