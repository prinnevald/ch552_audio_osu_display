/* SPDX-License-Identifier: MIT */
#ifndef AUDIO_H
#define AUDIO_H
#include <stdint.h>

void audio_init(void);
uint8_t audio_has_space(void);
void audio_push(uint8_t sample);

/* Called from the 32-kHz Timer2 ISR; consumes a new PCM byte every 4 ticks. */
void audio_tick_32khz(void);
#endif
