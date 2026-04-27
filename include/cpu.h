/*
 * 65816 CPU Emulation Interface
 */

#ifndef CPU_H
#define CPU_H

#include "zsnes.h"

/* Initialize CPU state */
int cpu_init(zsnes_emu_t *emu);

/* Deinitialize CPU */
void cpu_deinit(zsnes_emu_t *emu);

/* Execute one complete frame (~262 scanlines for NTSC) */
int cpu_execute_frame(zsnes_emu_t *emu);

/* Execute one CPU cycle */
int cpu_execute_cycle(zsnes_emu_t *emu);

/* Read/write memory (addresses are logical 24-bit) */
uint8_t  cpu_mem_read8(zsnes_emu_t *emu, uint32_t addr);
uint16_t cpu_mem_read16(zsnes_emu_t *emu, uint32_t addr);
void cpu_mem_write8(zsnes_emu_t *emu, uint32_t addr, uint8_t val);
void cpu_mem_write16(zsnes_emu_t *emu, uint32_t addr, uint16_t val);

/* Handle input events */
void cpu_handle_keydown(zsnes_emu_t *emu, uint8_t key);
void cpu_handle_keyup(zsnes_emu_t *emu, uint8_t key);

#endif /* CPU_H */
