/*
 * ZSNES for EYN-OS
 * Main entry point and EYN-OS integration
 * Package rebuild marker for the ROM read fix release.
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
static uint16_t scaled_framebuffer[320 * 200];

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

static void scale_framebuffer_to_gui(const uint16_t *src, int src_w, int src_h, uint16_t *dst, int dst_w, int dst_h) {
    if (!src || !dst || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) {
        return;
    }

    for (int y = 0; y < dst_h; ++y) {
        int src_y = (y * src_h) / dst_h;
        if (src_y >= src_h) {
            src_y = src_h - 1;
        }

        for (int x = 0; x < dst_w; ++x) {
            int src_x = (x * src_w) / dst_w;
            if (src_x >= src_w) {
                src_x = src_w - 1;
            }

            dst[y * dst_w + x] = src[src_y * src_w + src_x];
        }
    }
}

static int emulation_loop(void) {
    fprintf(stderr, "DEBUG: ENTERED emulation_loop()\n");
    fflush(stderr);
    
    /* Attach to the current tile first so the emulator replaces the parent view.
     * Fall back to a new GUI tile if attachment is not available.
     */
    fprintf(stderr, "DEBUG: Attempting gui_attach()...\n");
    fflush(stderr);
    gui_handle = gui_attach("ZSNES - Super Nintendo Emulator", "Running...");
    fprintf(stderr, "DEBUG: gui_attach() returned handle=%d\n", gui_handle);
    fflush(stderr);
    
    if (gui_handle < 0) {
        fprintf(stderr, "DEBUG: gui_attach failed, trying gui_create()...\n");
        fflush(stderr);
        gui_handle = gui_create("ZSNES - Super Nintendo Emulator", "Running...");
        fprintf(stderr, "DEBUG: gui_create() returned handle=%d\n", gui_handle);
        fflush(stderr);
    }
    if (gui_handle < 0) {
        fprintf(stderr, "Error: Failed to create/attach GUI window (handle=%d)\n", gui_handle);
        fflush(stderr);
        return -1;
    }

    fprintf(stderr, "DEBUG: GUI window initialized with handle %d\n", gui_handle);
    fflush(stderr);

    int ret = gui_set_continuous_redraw(gui_handle, 1);
    fprintf(stderr, "DEBUG: gui_set_continuous_redraw() returned %d\n", ret);
    fflush(stderr);

    gui_size_t content_size = {0, 0};
    if (gui_get_content_size(gui_handle, &content_size) < 0 || content_size.w <= 0 || content_size.h <= 0) {
        content_size.w = 320;
        content_size.h = 200;
    }
    if (content_size.w > 320) {
        content_size.w = 320;
    }
    if (content_size.h > 200) {
        content_size.h = 200;
    }

    int running = 1;
    int frame_count = 0;

    printf("Emulation started. Press Ctrl+Q or close window to quit.\n");
    printf("Controls: Arrow keys = D-Pad, Z=B, X=A, A=Y, S=X, Q=L, W=R, Enter=Start, Space=Select\n");
    fflush(stdout);

    while (running) {
        frame_count++;
        
        /* Begin frame */
        fprintf(stderr, "DEBUG: Frame %d - calling gui_begin()...\n", frame_count);
        fflush(stderr);
        int begin_ret = gui_begin(gui_handle);
        fprintf(stderr, "DEBUG: gui_begin() returned %d\n", begin_ret);
        fflush(stderr);

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
        fprintf(stderr, "DEBUG: Frame %d - calling ppu_render_frame()...\n", frame_count);
        fflush(stderr);
        int ppu_ret = ppu_render_frame(emu);
        fprintf(stderr, "DEBUG: ppu_render_frame() returned %d, framebuffer ptr=%p\n", ppu_ret, (void *)emu->ppu.framebuffer);
        fflush(stderr);
        
        if (ppu_ret < 0) {
            fprintf(stderr, "Warning: PPU rendering failed\n");
        }

        /* Clear and render to GUI window */
        gui_rgb_t bg_color = {0, 0, 0, 0};
        fprintf(stderr, "DEBUG: Calling gui_clear()...\n");
        fflush(stderr);
        gui_clear(gui_handle, &bg_color);

        /* Scale the PPU framebuffer to the GUI-supported blit size. */
        scale_framebuffer_to_gui(
            emu->ppu.framebuffer,
            SNES_WIDTH,
            SNES_HEIGHT,
            scaled_framebuffer,
            content_size.w,
            content_size.h);

        /* Blit the scaled framebuffer into the content area. */
        gui_blit_rgb565_t blit = {
            .src_w = content_size.w,
            .src_h = content_size.h,
            .pixels = scaled_framebuffer,
            .dst_w = 0,
            .dst_h = 0,
        };
        fprintf(stderr, "DEBUG: Calling gui_blit_rgb565() with framebuffer %p...\n", (void *)blit.pixels);
        fflush(stderr);
        int blit_ret = gui_blit_rgb565(gui_handle, &blit);
        fprintf(stderr, "DEBUG: gui_blit_rgb565() returned %d\n", blit_ret);
        fflush(stderr);

        /* Process input events */
        gui_event_t ev;
        while (gui_poll_event(gui_handle, &ev) > 0) {
            switch (ev.type) {
                case GUI_EVENT_KEY:
                    /* Key press */
                    cpu_handle_keydown(emu, ev.a);
                    break;
                case GUI_EVENT_KEY_UP:
                    /* Key release */
                    cpu_handle_keyup(emu, ev.a);
                    break;
                case GUI_EVENT_CLOSE:
                    running = 0;
                    break;
                default:
                    break;
            }
        }

        /* Present frame to display */
        fprintf(stderr, "DEBUG: Calling gui_present()...\n");
        fflush(stderr);
        int present_ret = gui_present(gui_handle);
        fprintf(stderr, "DEBUG: gui_present() returned %d\n", present_ret);
        fflush(stderr);

        /* Print debug every 60 frames (~1 second) */
        if (frame_count % 60 == 0) {
            fprintf(stderr, "DEBUG: Emulation running, frame %d, GUI handle=%d\n", frame_count, gui_handle);
            fflush(stderr);
        }

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

    fprintf(stderr, "DEBUG: About to call emulation_loop()...\n");
    fflush(stderr);
    
    /* Run emulation loop */
    int result = emulation_loop();
    
    fprintf(stderr, "DEBUG: emulation_loop() returned %d\n", result);
    fflush(stderr);

    /* Cleanup */
    cleanup_emulator();

    return result;
}
