/*
 * SNES PPU (Picture Processing Unit) Graphics Emulation
 */

#ifndef PPU_H
#define PPU_H

#include "zsnes.h"

/* Initialize PPU state */
int ppu_init(zsnes_emu_t *emu);

/* Deinitialize PPU */
void ppu_deinit(zsnes_emu_t *emu);

/* Render one scanline */
void ppu_render_scanline(zsnes_emu_t *emu, uint16_t scanline);

/* Render complete frame to framebuffer */
int ppu_render_frame(zsnes_emu_t *emu);

/* Handle PPU register writes (addresses 0x2100-0x213F) */
void ppu_write_reg(zsnes_emu_t *emu, uint8_t reg, uint8_t val);

/* Handle PPU register reads */
uint8_t ppu_read_reg(zsnes_emu_t *emu, uint8_t reg);

#endif /* PPU_H */
