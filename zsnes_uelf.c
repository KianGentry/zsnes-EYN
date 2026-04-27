/*
 * ZSNES for EYN-OS
 * Main entry point and EYN-OS integration
 *
 * This file serves as the EYN-OS userland entry point. It initializes
 * the emulator, handles command-line arguments, and runs the main
 * emulation loop.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <gui.h>

#include "zsnes.h"
#include "rom.h"
#include "cpu.h"
#include "ppu.h"
#include "smp.h"

/* Global emulator state */
static zsnes_emu_t *emu = NULL;
static int gui_handle = 0;

static void print_usage(void) {
    printf("Usage: zsnes_1_51 [ROM_PATH]\n\n");
    printf("Load and emulate a Super Nintendo ROM file.\n\n");
    printf("Supported formats:\n");
    printf("  .sfc  - Super Famicom (linear ROM)\n");
    printf("  .smc  - SNES cartridge (interleaved ROM)\n\n");
    printf("Example:\n");
    printf("  zsnes_1_51 /path/to/Super_Mario_World.sfc\n");
}

static int initialize_emulator(const char *rom_path) {
    /* Allocate emulator state */
    emu = (zsnes_emu_t *)malloc(sizeof(zsnes_emu_t));
    if (!emu) {
        fprintf(stderr, "Error: Failed to allocate emulator state\n");
        return -1;
    }

    memset(emu, 0, sizeof(zsnes_emu_t));

    /* Load ROM */
    if (rom_load(emu, rom_path) < 0) {
        fprintf(stderr, "Error: Failed to load ROM: %s\n", rom_path);
        free(emu);
        emu = NULL;
        return -1;
    }

    /* Initialize CPU, PPU, SMP */
    if (cpu_init(emu) < 0) {
        fprintf(stderr, "Error: Failed to initialize CPU\n");
        rom_unload(emu);
        free(emu);
        emu = NULL;
        return -1;
    }

    if (ppu_init(emu) < 0) {
        fprintf(stderr, "Error: Failed to initialize PPU\n");
        cpu_deinit(emu);
        rom_unload(emu);
        free(emu);
        emu = NULL;
        return -1;
    }

    if (smp_init(emu) < 0) {
        fprintf(stderr, "Error: Failed to initialize SMP\n");
        ppu_deinit(emu);
        cpu_deinit(emu);
        rom_unload(emu);
        free(emu);
        emu = NULL;
        return -1;
    }

    return 0;
}

static void cleanup_emulator(void) {
    if (!emu)
        return;

    smp_deinit(emu);
    ppu_deinit(emu);
    cpu_deinit(emu);
    rom_unload(emu);
    free(emu);
    emu = NULL;
}

static int emulation_loop(void) {
    /* Create GUI window using EYN-OS API
     * Note: gui_create() creates a new tile; gui_attach() uses existing tile
     * For an emulator, creating a new tile is appropriate
     */
    gui_handle = gui_create("ZSNES - Super Nintendo Emulator", "Running...");
    if (gui_handle < 0) {
        fprintf(stderr, "Error: Failed to create GUI window\n");
        return -1;
    }

    int running = 1;
    gui_event_t event;

    printf("Emulation started. Press Ctrl+Q or close window to quit.\n");
    printf("Controls: Arrow keys = D-Pad, Z=B, X=A, A=Y, S=X, Q=L, W=R, Enter=Start, Space=Select\n");

    while (running) {
        /* Begin frame */
        gui_begin(gui_handle);

        /* Execute CPU for one frame (~262 scanlines for NTSC) */
        if (cpu_execute_frame(emu) < 0) {
            fprintf(stderr, "Error: CPU execution failed\n");
            running = 0;
            break;
        }

        /* Generate audio for this frame */
        if (smp_generate_frame(emu) < 0) {
            fprintf(stderr, "Warning: SMP audio generation failed\n");
        }

        /* Render PPU framebuffer */
        if (ppu_render_frame(emu) < 0) {
            fprintf(stderr, "Warning: PPU rendering failed\n");
        }

        /* Clear and render to GUI window
         * The PPU framebuffer is 256x224 in 15-bit RGB
         * We can blit it directly to the GUI using gui_blit_rgb565()
         * (may need to convert 5555 to 565 format)
         */
        gui_rgb_t bg_color = {0, 0, 0, 0};
        gui_clear(gui_handle, &bg_color);

        /* Blit the PPU framebuffer (256x224) scaled 2x to the window (512x448) */
        gui_blit_rgb565_t blit = {
            .src_w = 256,
            .src_h = 224,
            .pixels = (uint16_t *)emu->ppu.framebuffer,
            .dst_w = 512,
            .dst_h = 448,
        };
        gui_blit_rgb565(gui_handle, &blit);

        /* Draw frame counter and status */
        gui_char_t char_cmd = {
            .x = 10,
            .y = 10,
            .ch = 'F',
            .r = 255,
            .g = 255,
            .b = 255,
        };
        gui_draw_char(gui_handle, &char_cmd);

        /* Process input events */
        gui_event_t ev;
        while (gui_poll_event(gui_handle, &ev) > 0) {
            switch (ev.type) {
                case GUI_EVENT_KEY:
                    /* Key press */
                    cpu_handle_keydown(emu, (uint8_t)ev.a);
                    break;
                case GUI_EVENT_KEY_UP:
                    /* Key release */
                    cpu_handle_keyup(emu, (uint8_t)ev.a);
                    break;
                case GUI_EVENT_CLOSE:
                    running = 0;
                    break;
                default:
                    break;
            }
        }

        /* Present frame to display */
        gui_present(gui_handle);

        /* Approximate frame rate limiting */
        usleep(16000);  /* ~16ms for ~60 FPS */
    }

    /* Clean up */
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char *rom_path = argv[1];

    /* Check for help flag */
    if (strcmp(rom_path, "-h") == 0 || strcmp(rom_path, "--help") == 0) {
        print_usage();
        return 0;
    }

    /* Initialize emulator with ROM */
    if (initialize_emulator(rom_path) < 0) {
        return 1;
    }

    /* Run emulation loop */
    int result = emulation_loop();

    /* Cleanup */
    cleanup_emulator();

    return result;
}
