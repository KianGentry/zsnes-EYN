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

/* Window dimensions */
#define WINDOW_WIDTH 512
#define WINDOW_HEIGHT 448
#define BASE_RES_WIDTH 256
#define BASE_RES_HEIGHT 224

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
    /* Create GUI window */
    if (gui_create(WINDOW_WIDTH, WINDOW_HEIGHT, "ZSNES") < 0) {
        fprintf(stderr, "Error: Failed to create GUI window\n");
        return -1;
    }

    int running = 1;
    gui_event_t event;

    while (running) {
        /* Begin frame */
        gui_begin();

        /* Execute CPU for one frame (~262 scanlines) */
        /* This will internally handle PPU rendering and SMP audio */
        if (cpu_execute_frame(emu) < 0) {
            fprintf(stderr, "Error: CPU execution failed\n");
            running = 0;
            break;
        }

        /* Render PPU framebuffer to GUI */
        /* (Implementation: convert emulated framebuffer to GUI canvas) */
        ppu_render_frame(emu);

        /* Process input events */
        while (gui_poll_event(&event) == 0) {
            switch (event.type) {
                case GUI_EVENT_KEYDOWN:
                    cpu_handle_keydown(emu, event.key);
                    break;
                case GUI_EVENT_KEYUP:
                    cpu_handle_keyup(emu, event.key);
                    break;
                case GUI_EVENT_WINDOW_CLOSE:
                    running = 0;
                    break;
                default:
                    break;
            }
        }

        /* Present frame */
        gui_present();
    }

    gui_deinit();
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
