/*
 * SNES SMP (Sound Processor) and SPC700 Audio Emulation
 */

#ifndef SMP_H
#define SMP_H

#include "zsnes.h"

/* Initialize SMP state and audio output */
int smp_init(zsnes_emu_t *emu);

/* Deinitialize SMP and audio */
void smp_deinit(zsnes_emu_t *emu);

/* Execute SMP for one cycle */
int smp_execute_cycle(zsnes_emu_t *emu);

/* Generate audio samples for one frame and output to system */
int smp_generate_frame(zsnes_emu_t *emu);

/* Set audio sample rate for output */
void smp_set_sample_rate(uint32_t rate);

#endif /* SMP_H */
