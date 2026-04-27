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

    /* TODO: Convert 256x224 framebuffer to GUI format and render
     *
     * The framebuffer is 256x224 pixels in 15-bit RGB format.
     * This function should:
     * 1. Scale/filter the framebuffer to fit the GUI window
     * 2. Use gui_* functions to draw the scaled output
     * 3. Handle color format conversion if needed
     *
     * For now, just clear the screen to demonstrate GUI integration.
     */

    /* Placeholder: clear with dark color */
    for (int y = 0; y < SNES_HEIGHT; y++) {
        for (int x = 0; x < SNES_WIDTH; x++) {
            emu->ppu.framebuffer[y * SNES_WIDTH + x] = 0x0000;  /* Black */
        }
    }

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
