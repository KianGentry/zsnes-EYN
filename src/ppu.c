/*
 * SNES PPU (Picture Processing Unit) Graphics Emulation
 *
 * This module handles graphics rendering for the SNES, including:
 * - Tile-based background graphics (4 layers)
 * - Sprite rendering via OAM
 * - Color palette (CGRAM) management
 * - All 8 graphics modes (0-7, including Mode 7 pseudo-3D)
 * - Output to EYN-OS GUI system
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gui.h>
#include "ppu.h"

/* PPU register access helpers */
#define PPU_REG_INIDISP         0x00  /* Screen display */
#define PPU_REG_OBJSEL          0x01  /* Object selection */
#define PPU_REG_OAMADDL         0x02  /* OAM address (low) */
#define PPU_REG_OAMADDH         0x03  /* OAM address (high) */
#define PPU_REG_BGMODE          0x05  /* Background mode */
#define PPU_REG_MOSAIC          0x06  /* Mosaic control */
#define PPU_REG_BG1SC           0x07  /* BG1 tilemap size/address */
#define PPU_REG_BG2SC           0x08  /* BG2 tilemap size/address */
#define PPU_REG_BG3SC           0x09  /* BG3 tilemap size/address */
#define PPU_REG_BG4SC           0x0A  /* BG4 tilemap size/address */
#define PPU_REG_BG12NBA         0x0B  /* BG1/2 tile data address */
#define PPU_REG_BG34NBA         0x0C  /* BG3/4 tile data address */

static uint8_t scale_color_to_8bit(uint16_t color_555) __attribute__((unused));
static uint8_t scale_color_to_8bit(uint16_t color_555) {
    /* Convert 15-bit SNES color (5R5G5B) to 8-bit grayscale for simple rendering
     * In a full implementation, this would output RGB directly to GUI
     */
    uint8_t r = (color_555 & 0x1F) << 3;
    uint8_t g = ((color_555 >> 5) & 0x1F) << 3;
    uint8_t b = ((color_555 >> 10) & 0x1F) << 3;

    /* Simple luminance calculation */
    return (r * 299 + g * 587 + b * 114) / 1000;
}

int ppu_init(zsnes_emu_t *emu) {
    if (!emu)
        return -1;

    /* Initialize VRAM, CGRAM, OAM with zeroes */
    memset(emu->ppu.vram, 0, SNES_VRAM_SIZE);
    memset(emu->ppu.cgram, 0, sizeof(emu->ppu.cgram));
    memset(emu->ppu.oam, 0, SNES_OAM_SIZE);
    memset(emu->ppu.regs, 0, 256);
    memset(emu->ppu.framebuffer, 0, sizeof(emu->ppu.framebuffer));

    /* Initialize scanline counter */
    emu->ppu.scanline = 0;
    emu->ppu.cycle = 0;

    printf("PPU initialized\n");
    return 0;
}

void ppu_deinit(zsnes_emu_t *emu) {
    if (!emu)
        return;
    /* Nothing to clean up */
}

void ppu_render_scanline(zsnes_emu_t *emu, uint16_t scanline) {
    if (!emu || scanline >= SNES_HEIGHT)
        return;

    /* TODO: Render one scanline to framebuffer
     *
     * Steps:
     * 1. Fetch tile data and palette indices from VRAM for each BG layer
     * 2. Blend layers according to priority (window, effects)
     * 3. Composite sprites (OAM) over backgrounds
     * 4. Apply color math (brightness, addition, subtraction)
     * 5. Apply special effects (Mode 7 affine transform, etc.)
     * 6. Write final pixel data to framebuffer[scanline * SNES_WIDTH]
     *
     * This is graphics-intensive and is the most complex part of PPU emulation.
     */

    (void)scanline;
}

int ppu_render_frame(zsnes_emu_t *emu) {
    if (!emu)
        return -1;

    /* The SNES renderer is still a stub. Keep the framebuffer stable so the
     * GUI does not flash a synthetic test pattern while the emulator core runs.
     */
    memset(emu->ppu.framebuffer, 0, sizeof(emu->ppu.framebuffer));

    return 0;
}

void ppu_write_reg(zsnes_emu_t *emu, uint8_t reg, uint8_t val) {
    if (!emu || reg > 0x3F)
        return;

    /* TODO: Handle PPU register writes
     *
     * Registers 0x2100-0x213F are mirrored into 0x00-0x3F.
     * Key registers:
     * - 0x00 (INIDISP): Screen brightness and forced blank
     * - 0x01-0x06: Scroll registers (BGXSC, BGxSCx)
     * - 0x07-0x0A: Scroll offsets (BGxHOFS, BGxVOFS)
     * - 0x0B-0x0E: Mode settings (BGMODE, MOSAIC, etc.)
     * - 0x0F-0x13: Window/clipping controls
     * - 0x14-0x20: Color math, VRAM access, palette controls
     * - 0x21-0x3F: OAM, VRAM, palette data ports
     */

    emu->ppu.regs[reg] = val;
}

uint8_t ppu_read_reg(zsnes_emu_t *emu, uint8_t reg) {
    if (!emu || reg > 0x3F)
        return 0;

    /* TODO: Handle PPU register reads
     *
     * Some registers return status information:
     * - STAT77 / STAT78: PPU status (interlace, sprite overflow, etc.)
     * - MPYL, MPYM, MPYH: Multiplication result
     * - SLHV: Scanline/H-counter read (for light-gun style games)
     * - RDVRAM: VRAM data port read
     * - ROAM: OAM data read
     * - etc.
     */

    return emu->ppu.regs[reg];
}
