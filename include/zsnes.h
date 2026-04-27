/*
 * ZSNES Core Emulator Definitions
 * Defines the emulator state and core structures
 */

#ifndef ZSNES_H
#define ZSNES_H

#include <stdint.h>

/* SNES memory map sizes */
#define SNES_ROM_MAX        (4 * 1024 * 1024)  /* 4 MB max ROM size */
#define SNES_RAM_SIZE       (128 * 1024)       /* 128 KB SNES RAM */
#define SNES_VRAM_SIZE      (64 * 1024)        /* 64 KB VRAM */
#define SNES_CGRAM_SIZE     (512)              /* 512 B CGRAM (palette) */
#define SNES_OAM_SIZE       (512 + 32)         /* 544 B OAM (sprites) */

/* CPU clock frequency */
#define CPU_CLOCK_NTSC      (3579545)          /* Hz, NTSC */
#define CPU_CLOCK_PAL       (3546894)          /* Hz, PAL */

/* Frame rates */
#define FPS_NTSC            (59.727)           /* Hz */
#define FPS_PAL             (50.0)             /* Hz */
#define SCANLINES_NTSC      (262)
#define SCANLINES_PAL       (312)

/* SNES resolution */
#define SNES_WIDTH          (256)
#define SNES_HEIGHT         (224)

/* ROM format detection */
typedef enum {
    ROM_FORMAT_UNKNOWN = 0,
    ROM_FORMAT_SFC,                 /* Linear ROM */
    ROM_FORMAT_SMC,                 /* Interleaved ROM */
} rom_format_t;

/* 65816 CPU state */
typedef struct {
    uint16_t a;                     /* Accumulator */
    uint16_t x, y;                  /* Index registers */
    uint16_t s;                     /* Stack pointer */
    uint16_t d;                     /* Direct page register */
    uint16_t pc;                    /* Program counter */
    uint8_t  p;                     /* Processor status */
    uint8_t  db;                    /* Data bank */
    uint8_t  pb;                    /* Program bank */
    
    /* Flags */
    uint8_t  m_flag : 1;            /* Memory/accumulator size (1=8-bit, 0=16-bit) */
    uint8_t  x_flag : 1;            /* Index register size (1=8-bit, 0=16-bit) */
} cpu_state_t;

/* 6502 SMP (Sound processor) CPU state */
typedef struct {
    uint16_t pc;
    uint8_t  a, x, y;
    uint8_t  s;
    uint8_t  p;
} smp_state_t;

/* PPU state */
typedef struct {
    /* Current scanline and cycle */
    uint16_t scanline;
    uint16_t cycle;
    
    /* PPU registers (INIDISP, OBJSEL, OAMADDL, OAMADDH, etc.) */
    uint8_t  regs[256];             /* PPU register mirror */
    
    /* VRAM, CGRAM, OAM */
    uint8_t  vram[SNES_VRAM_SIZE];
    uint16_t cgram[256];            /* 256 x 15-bit palette entries */
    uint8_t  oam[SNES_OAM_SIZE];
    
    /* Current framebuffer */
    uint16_t framebuffer[SNES_WIDTH * SNES_HEIGHT];
} ppu_state_t;

/* ROM metadata */
typedef struct {
    uint8_t  *data;                 /* ROM data */
    size_t   size;
    rom_format_t format;
    
    /* Cartridge header (parsed) */
    char     title[21];             /* Game title */
    uint8_t  map_mode;              /* Memory map mode */
    uint8_t  cartridge_type;        /* Cartridge type */
    uint8_t  rom_size_flag;         /* ROM size flag */
    uint8_t  ram_size_flag;         /* RAM size flag */
} rom_t;

/* Complete emulator state */
typedef struct {
    cpu_state_t cpu;
    smp_state_t smp;
    ppu_state_t ppu;
    rom_t       rom;
    
    /* Main RAM and SRAM */
    uint8_t     ram[SNES_RAM_SIZE];
    uint8_t     sram[64 * 1024];    /* Save RAM (up to 64 KB) */
    
    /* SMP audio RAM */
    uint8_t     smp_ram[64 * 1024]; /* SMP owns 64 KB */
    
    /* Timing */
    uint32_t    frame_count;
    uint32_t    cycle_count;
    
    /* Input state */
    uint16_t    controller1;
    uint16_t    controller2;
    
    /* System flags */
    uint8_t     pal_mode;           /* 1 = PAL, 0 = NTSC */
    uint8_t     running;
} zsnes_emu_t;

/* SNES controller button bits */
#define BUTTON_B        (1 << 0)
#define BUTTON_Y        (1 << 1)
#define BUTTON_SELECT   (1 << 2)
#define BUTTON_START    (1 << 3)
#define BUTTON_UP       (1 << 4)
#define BUTTON_DOWN     (1 << 5)
#define BUTTON_LEFT     (1 << 6)
#define BUTTON_RIGHT    (1 << 7)
#define BUTTON_A        (1 << 8)
#define BUTTON_X        (1 << 9)
#define BUTTON_L        (1 << 10)
#define BUTTON_R        (1 << 11)

#endif /* ZSNES_H */
