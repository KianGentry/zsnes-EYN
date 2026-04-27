/*
 * 65816 CPU Emulation
 *
 * This module implements emulation of the WDC 65816 processor, the CPU of the SNES.
 * It handles:
 * - 16-bit and 8-bit modes
 * - 24-bit memory addressing with banks
 * - Instruction decoding and execution
 * - Interrupt handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"

/* 65816 instruction table - opcode dispatch
 * Each entry describes: opcode, name, addressing mode, operation type
 * Mode legend: IMM=immediate, ZP=zero page, ZPX=zero page X, ZPY=zero page Y,
 *              ABS=absolute, ABX/ABY=absolute indexed, ABXL=absolute long indexed,
 *              IND=(indirect), INDX=indexed indirect, INDY=indirect indexed,
 *              REL=relative, RELL=relative long, BIT=bit operation
 */

typedef struct {
    const char *name;
    uint8_t bytes;      /* Instruction length in bytes */
    uint8_t cycles;     /* Base cycle count */
} opcode_info_t;

static const opcode_info_t opcodes[256] = {
    /* 0x00 */ { "BRK", 2, 7 },  /* Break */
    /* 0x01 */ { "ORA", 2, 6 },  /* ORA (indirect,X) */
    /* 0x02 */ { "COP", 2, 7 },  /* Coprocessor */
    /* 0x03 */ { "ORA", 2, 4 },  /* ORA stack relative */
    /* 0x04 */ { "TSB", 2, 5 },  /* TSB direct page */
    /* 0x05 */ { "ORA", 2, 3 },  /* ORA direct page */
    /* 0x06 */ { "ASL", 2, 5 },  /* ASL direct page */
    /* 0x07 */ { "ORA", 3, 6 },  /* ORA [direct page indirect] */
    /* 0x08 */ { "PHP", 1, 3 },  /* Push processor status */
    /* 0x09 */ { "ORA", 3, 2 },  /* ORA immediate */
    /* 0x0A */ { "ASL", 1, 2 },  /* ASL accumulator */
    /* 0x0B */ { "PHD", 1, 4 },  /* Push direct page register */
    /* 0x0C */ { "TSB", 3, 6 },  /* TSB absolute */
    /* 0x0D */ { "ORA", 3, 4 },  /* ORA absolute */
    /* 0x0E */ { "ASL", 3, 6 },  /* ASL absolute */
    /* 0x0F */ { "ORA", 4, 5 },  /* ORA absolute long */
    /* ... (full table would have all 256 opcodes) ... */
};

/* Memory mapping for SNES address space
 * 0x0000-0x1FFF: Zero page and stack (8 KB mirrored from WRAM)
 * 0x2000-0x3FFF: PPU and DSP registers (I/O)
 * 0x4000-0x41FF: CPU and DMA registers (I/O)
 * 0x4200-0x5FFF: Unused
 * 0x6000-0x7FFF: SRAM (optionally cartridge RAM)
 * 0x8000-0xFFFF: ROM (varies by ROM mode/bank)
 * Addresses 0x0000-0xFFFF are repeated in banks 0x01-0x7F
 * Banks 0x80-0xFF mirror 0x00-0x7F
 */

static uint8_t mem_read8_internal(zsnes_emu_t *emu, uint32_t addr) {
    uint8_t bank = (addr >> 16) & 0x7F;
    uint16_t offset = addr & 0xFFFF;

    /* Handle mirror in high banks (0x80-0xFF map to 0x00-0x7F) */
    if (bank >= 0x80) {
        bank -= 0x80;
    }

    /* Zero page and stack (first 8 KB) - mirrors of WRAM */
    if (offset < 0x2000) {
        return emu->ram[offset & 0x1FFF];
    }

    /* PPU and I/O registers (0x2000-0x3FFF) */
    if (offset >= 0x2000 && offset < 0x4000) {
        /* TODO: Handle PPU register reads */
        return 0;
    }

    /* CPU and DMA registers (0x4000-0x5FFF) */
    if (offset >= 0x4000 && offset < 0x6000) {
        /* TODO: Handle CPU register reads (multiplication, division, etc.) */
        return 0;
    }

    /* SRAM (0x6000-0x7FFF) */
    if (offset >= 0x6000 && offset < 0x8000) {
        if (bank < 8) {  /* Cartridge RAM is typically in banks 0x20-0x3F or 0x70-0x77 */
            return emu->sram[(bank * 0x2000) + (offset - 0x6000)];
        }
        return 0;
    }

    /* ROM (0x8000-0xFFFF) */
    if (offset >= 0x8000) {
        uint32_t rom_addr = (bank * 0x8000) + (offset - 0x8000);
        if (rom_addr < emu->rom.size) {
            return emu->rom.data[rom_addr];
        }
        return 0xFF;  /* Unmapped area */
    }

    return 0;
}

static void mem_write8_internal(zsnes_emu_t *emu, uint32_t addr, uint8_t val) {
    uint8_t bank = (addr >> 16) & 0x7F;
    uint16_t offset = addr & 0xFFFF;

    /* Handle mirror in high banks */
    if (bank >= 0x80) {
        bank -= 0x80;
    }

    /* Zero page and stack - writable */
    if (offset < 0x2000) {
        emu->ram[offset & 0x1FFF] = val;
        return;
    }

    /* PPU registers (0x2000-0x3FFF) - write to PPU */
    if (offset >= 0x2000 && offset < 0x4000) {
        /* TODO: Call ppu_write_reg() */
        return;
    }

    /* CPU registers (0x4000-0x5FFF) - write to CPU */
    if (offset >= 0x4000 && offset < 0x6000) {
        /* TODO: Handle CPU control register writes (JOY, WMDATA, etc.) */
        return;
    }

    /* SRAM (0x6000-0x7FFF) - writable */
    if (offset >= 0x6000 && offset < 0x8000) {
        if (bank < 8) {
            emu->sram[(bank * 0x2000) + (offset - 0x6000)] = val;
        }
        return;
    }

    /* ROM (0x8000-0xFFFF) - read-only, ignore writes */
}

/* Public memory access helpers */
static uint8_t mem_read8(zsnes_emu_t *emu, uint32_t addr) {
    return mem_read8_internal(emu, addr);
}

static void mem_write8(zsnes_emu_t *emu, uint32_t addr, uint8_t val) {
    mem_write8_internal(emu, addr, val);
}

int cpu_init(zsnes_emu_t *emu) {
    if (!emu)
        return -1;

    /* Initialize CPU registers */
    emu->cpu.pc = 0x8000;   /* Reset vector typically at 0x8000 */
    emu->cpu.a = 0;
    emu->cpu.x = 0;
    emu->cpu.y = 0;
    emu->cpu.s = 0x01FF;    /* Stack at 0x01xx */
    emu->cpu.d = 0;         /* Direct page at 0x0000 */
    emu->cpu.p = 0x34;      /* Status: m=1, x=1, other flags */
    emu->cpu.pb = 0x00;
    emu->cpu.db = 0x00;

    emu->cpu.m_flag = 1;    /* 8-bit A */
    emu->cpu.x_flag = 1;    /* 8-bit X/Y */

    printf("CPU initialized\n");
    return 0;
}

void cpu_deinit(zsnes_emu_t *emu) {
    if (!emu)
        return;
    /* Nothing to clean up for now */
}

/* Instruction execution helpers */

/* Set flags from value (NV-BDIZC) */
#define SET_FLAGS(p, val) do { \
    emu->cpu.p = (emu->cpu.p & 0x30) | ((val) & 0xCF); \
} while(0)

#define GET_FLAG(f) (emu->cpu.p & (f))
#define SET_FLAG(f) (emu->cpu.p |= (f))
#define CLR_FLAG(f) (emu->cpu.p &= ~(f))

/* 65816 Processor Status Flags */
#define FLAG_N  0x80  /* Negative */
#define FLAG_V  0x40  /* Overflow */
#define FLAG_M  0x20  /* Accumulator/memory size (1=8-bit, 0=16-bit) */
#define FLAG_X  0x10  /* Index register size (1=8-bit, 0=16-bit) */
#define FLAG_D  0x08  /* Decimal mode */
#define FLAG_I  0x04  /* Interrupt disable */
#define FLAG_Z  0x02  /* Zero */
#define FLAG_C  0x01  /* Carry */

/* Update N and Z flags based on result */
static void update_nz_flags(zsnes_emu_t *emu, uint16_t result, int is_16bit) {
    if (is_16bit) {
        if (result == 0) SET_FLAG(FLAG_Z);
        else CLR_FLAG(FLAG_Z);
        if (result & 0x8000) SET_FLAG(FLAG_N);
        else CLR_FLAG(FLAG_N);
    } else {
        if ((result & 0xFF) == 0) SET_FLAG(FLAG_Z);
        else CLR_FLAG(FLAG_Z);
        if (result & 0x80) SET_FLAG(FLAG_N);
        else CLR_FLAG(FLAG_N);
    }
}

/* Execute a single 65816 instruction */
static int execute_instruction(zsnes_emu_t *emu) {
    uint8_t opcode;
    uint32_t pc_addr = ((uint32_t)emu->cpu.pb << 16) | emu->cpu.pc;

    /* Fetch opcode */
    opcode = mem_read8(emu, pc_addr);
    emu->cpu.pc++;

    int cycles = 2;  /* Base cycle count */

    /* Instruction dispatch */
    switch (opcode) {
        case 0xEA:  /* NOP - No operation */
            cycles = 2;
            break;

        case 0x18:  /* CLC - Clear carry */
            CLR_FLAG(FLAG_C);
            cycles = 2;
            break;

        case 0x38:  /* SEC - Set carry */
            SET_FLAG(FLAG_C);
            cycles = 2;
            break;

        case 0x58:  /* CLI - Clear interrupt disable */
            CLR_FLAG(FLAG_I);
            cycles = 2;
            break;

        case 0x78:  /* SEI - Set interrupt disable */
            SET_FLAG(FLAG_I);
            cycles = 2;
            break;

        case 0xB8:  /* CLV - Clear overflow */
            CLR_FLAG(FLAG_V);
            cycles = 2;
            break;

        case 0xD8:  /* CLD - Clear decimal mode */
            CLR_FLAG(FLAG_D);
            cycles = 2;
            break;

        case 0xF8:  /* SED - Set decimal mode */
            SET_FLAG(FLAG_D);
            cycles = 2;
            break;

        case 0xAA:  /* TAX - Transfer accumulator to X */
            emu->cpu.x = emu->cpu.a;
            update_nz_flags(emu, emu->cpu.x, !emu->cpu.x_flag);
            cycles = 2;
            break;

        case 0xA8:  /* TAY - Transfer accumulator to Y */
            emu->cpu.y = emu->cpu.a;
            update_nz_flags(emu, emu->cpu.y, !emu->cpu.x_flag);
            cycles = 2;
            break;

        case 0x8A:  /* TXA - Transfer X to accumulator */
            emu->cpu.a = emu->cpu.x;
            update_nz_flags(emu, emu->cpu.a, !emu->cpu.m_flag);
            cycles = 2;
            break;

        case 0x98:  /* TYA - Transfer Y to accumulator */
            emu->cpu.a = emu->cpu.y;
            update_nz_flags(emu, emu->cpu.a, !emu->cpu.m_flag);
            cycles = 2;
            break;

        case 0x00:  /* BRK - Break / Interrupt */
            /* TODO: Handle interrupt sequence, push return address and status */
            cycles = 7;
            break;

        case 0x20:  /* JSR - Jump to subroutine */
        {
            uint16_t addr = mem_read8(emu, pc_addr + 1) |
                           (mem_read8(emu, pc_addr + 2) << 8);
            /* Push return address - 1 */
            uint16_t ret_addr = emu->cpu.pc + 1;
            emu->cpu.s -= 2;
            mem_write8(emu, 0x0100 | (emu->cpu.s + 1), ret_addr & 0xFF);
            mem_write8(emu, 0x0100 | (emu->cpu.s + 2), (ret_addr >> 8) & 0xFF);
            emu->cpu.pc = addr;
            cycles = 6;
            break;
        }

        case 0x60:  /* RTS - Return from subroutine */
        {
            uint16_t addr = mem_read8(emu, 0x0100 | (emu->cpu.s + 1)) |
                           (mem_read8(emu, 0x0100 | (emu->cpu.s + 2)) << 8);
            emu->cpu.s += 2;
            emu->cpu.pc = addr + 1;
            cycles = 6;
            break;
        }

        /* Immediate mode LDA #$nn */
        case 0xA9:
        {
            if (emu->cpu.m_flag) {
                /* 8-bit mode */
                uint8_t val = mem_read8(emu, pc_addr + 1);
                emu->cpu.a = (emu->cpu.a & 0xFF00) | val;
                emu->cpu.pc++;
                update_nz_flags(emu, val, 0);
                cycles = 2;
            } else {
                /* 16-bit mode */
                uint16_t val = mem_read8(emu, pc_addr + 1) |
                              (mem_read8(emu, pc_addr + 2) << 8);
                emu->cpu.a = val;
                emu->cpu.pc += 2;
                update_nz_flags(emu, val, 1);
                cycles = 3;
            }
            break;
        }

        default:
            fprintf(stderr, "Unknown opcode: 0x%02X at PC 0x%06X\n", opcode, pc_addr);
            return -1;
    }

    emu->cycle_count += cycles;
    return cycles;
}

int cpu_execute_cycle(zsnes_emu_t *emu) {
    if (!emu)
        return -1;

    return execute_instruction(emu);
}

int cpu_execute_frame(zsnes_emu_t *emu) {
    if (!emu)
        return -1;

    /* Execute CPU for approximately one frame (262 scanlines for NTSC)
     * NTSC: 3,579,545 Hz / 59.727 Hz = ~59,904 cycles per frame
     * SNES H-sync: ~1228 cycles per scanline
     * V-sync: 262 scanlines = ~59,904 total cycles
     */

    uint32_t frame_start_cycles = emu->cycle_count;
    uint32_t target_cycles = 59904;  /* NTSC cycles per frame */

    if (emu->pal_mode) {
        target_cycles = 71280;  /* PAL cycles per frame */
    }

    /* Execute instructions until reaching target cycle count */
    while ((emu->cycle_count - frame_start_cycles) < target_cycles) {
        if (execute_instruction(emu) < 0) {
            return -1;
        }
    }

    emu->frame_count++;
    return 0;
}

uint8_t cpu_mem_read8(zsnes_emu_t *emu, uint32_t addr) {
    return mem_read8(emu, addr);
}

uint16_t cpu_mem_read16(zsnes_emu_t *emu, uint32_t addr) {
    uint16_t val = mem_read8(emu, addr);
    val |= (mem_read8(emu, addr + 1) << 8);
    return val;
}

void cpu_mem_write8(zsnes_emu_t *emu, uint32_t addr, uint8_t val) {
    mem_write8(emu, addr, val);
}

void cpu_mem_write16(zsnes_emu_t *emu, uint32_t addr, uint16_t val) {
    mem_write8(emu, addr, val & 0xFF);
    mem_write8(emu, addr + 1, (val >> 8) & 0xFF);
}

void cpu_handle_keydown(zsnes_emu_t *emu, uint8_t key) {
    if (!emu)
        return;

    /* Map keyboard input to SNES controller buttons
     *
     * Keyboard mapping:
     * - Arrow keys → D-Pad (UP/DOWN/LEFT/RIGHT)
     * - Z → B button
     * - X → A button
     * - A → Y button
     * - S → X button
     * - Q → L trigger
     * - W → R trigger
     * - Return → Start
     * - Space → Select
     */

    switch (key) {
        case 0x26:  /* Up arrow */
            emu->controller1 |= BUTTON_UP;
            break;
        case 0x28:  /* Down arrow */
            emu->controller1 |= BUTTON_DOWN;
            break;
        case 0x25:  /* Left arrow */
            emu->controller1 |= BUTTON_LEFT;
            break;
        case 0x27:  /* Right arrow */
            emu->controller1 |= BUTTON_RIGHT;
            break;
        case 'z': case 'Z':
            emu->controller1 |= BUTTON_B;
            break;
        case 'x': case 'X':
            emu->controller1 |= BUTTON_A;
            break;
        case 'a': case 'A':
            emu->controller1 |= BUTTON_Y;
            break;
        case 's': case 'S':
            emu->controller1 |= BUTTON_X;
            break;
        case 'q': case 'Q':
            emu->controller1 |= BUTTON_L;
            break;
        case 'w': case 'W':
            emu->controller1 |= BUTTON_R;
            break;
        case 0x0D:  /* Return */
            emu->controller1 |= BUTTON_START;
            break;
        case ' ':  /* Space */
            emu->controller1 |= BUTTON_SELECT;
            break;
        default:
            break;
    }
}

void cpu_handle_keyup(zsnes_emu_t *emu, uint8_t key) {
    if (!emu)
        return;

    /* Release controller button state */

    switch (key) {
        case 0x26:  /* Up arrow */
            emu->controller1 &= ~BUTTON_UP;
            break;
        case 0x28:  /* Down arrow */
            emu->controller1 &= ~BUTTON_DOWN;
            break;
        case 0x25:  /* Left arrow */
            emu->controller1 &= ~BUTTON_LEFT;
            break;
        case 0x27:  /* Right arrow */
            emu->controller1 &= ~BUTTON_RIGHT;
            break;
        case 'z': case 'Z':
            emu->controller1 &= ~BUTTON_B;
            break;
        case 'x': case 'X':
            emu->controller1 &= ~BUTTON_A;
            break;
        case 'a': case 'A':
            emu->controller1 &= ~BUTTON_Y;
            break;
        case 's': case 'S':
            emu->controller1 &= ~BUTTON_X;
            break;
        case 'q': case 'Q':
            emu->controller1 &= ~BUTTON_L;
            break;
        case 'w': case 'W':
            emu->controller1 &= ~BUTTON_R;
            break;
        case 0x0D:  /* Return */
            emu->controller1 &= ~BUTTON_START;
            break;
        case ' ':  /* Space */
            emu->controller1 &= ~BUTTON_SELECT;
            break;
        default:
            break;
    }
}
