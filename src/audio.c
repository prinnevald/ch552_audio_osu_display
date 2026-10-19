/* SPDX-License-Identifier: MIT
 * CH552 composite USB HID + CDC audio: experimental v6 8-kHz PDM.
 * GPIO P3.0 -> external BC337, 100-ohm + 17-ohm magnetic buzzer.
 * Timer2 is called 32,000 times/sec. 4 PDM bits per 8,000Hz PCM sample.
 * NO Timer0 or PWM peripheral required.
 * USB CDC incoming format: unsigned 8-bit PCM, mono, 8000 Hz.
 */
#include <stdint.h>
#include "ch552.h"
#include "audio.h"

#define AUDIO_FIFO_LEN   192u
#define START_THRESHOLD   96u
/* The 192-byte FIFO needs explicit wraparound (not a power of two). */

/* Set to zero to disable quiet detection; normally 128 is PCM silence. */
#define SILENCE_BAND       5u
#define QUIET_SAMPLES    32u  /* 4 ms of near-silence before muting */

static __xdata uint8_t audio_fifo[AUDIO_FIFO_LEN];
static volatile __data uint8_t audio_head = 0u;
static volatile __data uint8_t audio_tail = 0u;
static volatile __data uint8_t playing = 0u;

/* All these are accessed only from the Timer2 ISR after initialization. */
static __data uint8_t pdm_phase = 0u;
static __data uint8_t pdm_level = 128u;
static __data uint8_t pdm_accum = 0u;
static __data uint8_t pdm_muted = 1u;
static __data uint8_t quiet_run = 0u;
static __data uint16_t startup_samples = 0u;

/* Inline macro to avoid a non-reentrant function shared by main + ISR.
 * Each call passes a simple 8-bit index variable. */
#define NEXT_INDEX(pos) \
    ((uint8_t)(((pos) == (AUDIO_FIFO_LEN - 1u)) ? 0u : ((pos) + 1u)))

/* Directly address the PWM enable register to turn off old PWM mode. */
__sfr __at(0x9D) PWM_CTRL;

void audio_init(void)
{
    /* Return P3.0 to the tested GPIO configuration, with PWM disabled. */
    TR0 = 0;
    ET0 = 0;
    PWM_CTRL = 0u;
    PIN_FUNC &= (uint8_t)~0x04u;
    P3_MOD_OC &= (uint8_t)~0x01u;
    P3_DIR_PU |= 0x01u;
    P3_0 = 0;

    audio_head = 0u;
    audio_tail = 0u;
    playing = 0u;
    pdm_phase = 0u;
    pdm_level = 128u;
    pdm_accum = 0u;
    pdm_muted = 1u;
    quiet_run = 0u;
    startup_samples = 0u;
}

uint8_t audio_has_space(void)
{
    return (uint8_t)(NEXT_INDEX(audio_head) != audio_tail);
}

void audio_push(uint8_t sample)
{
    uint8_t next = NEXT_INDEX(audio_head);
    if (next == audio_tail) return;
    audio_fifo[audio_head] = sample;
    audio_head = next;
    if (!playing) {
        uint8_t buffered = (audio_head >= audio_tail)
            ? (uint8_t)(audio_head - audio_tail)
            : (uint8_t)(AUDIO_FIFO_LEN - audio_tail + audio_head);
        if (buffered >= START_THRESHOLD) playing = 1u;
    }
}

/* An 8-kHz PCM frame boundary, every fourth 32-kHz Timer2 interrupt. */
static void audio_next_sample(void)
{
    uint8_t sample;

    /* Preserve the known-good two-beep startup self-test. */
    if (startup_samples < 32000u) {
        ++startup_samples;
        if ((startup_samples >= 4000u && startup_samples < 12000u) ||
            (startup_samples >= 16000u && startup_samples < 24000u)) {
            /* 8 PCM frames ON / 8 OFF produces a 500 Hz square wave. */
            pdm_level = (startup_samples & 0x08u) ? 255u : 0u;
            pdm_muted = 0u;
        } else {
            pdm_level = 0u;
            pdm_muted = 1u;
        }
        return;
    }

    if (playing && audio_tail != audio_head) {
        sample = audio_fifo[audio_tail];
        audio_tail = NEXT_INDEX(audio_tail);
        pdm_level = sample;

        /* Idle/silence gating is ESSENTIAL for an unfiltered magnetic buzzer:
         * without it, PCM=128 gives a continuous 16 kHz 50%-density train.
         * The 4 ms hold avoids toggling the gate on every waveform zero-crossing.
         */
        if (sample >= (128u - SILENCE_BAND) &&
            sample <= (128u + SILENCE_BAND)) {
            if (quiet_run < QUIET_SAMPLES) ++quiet_run;
            if (quiet_run >= QUIET_SAMPLES) pdm_muted = 1u;
        } else {
            quiet_run = 0u;
            if (pdm_muted) pdm_accum = 0u;
            pdm_muted = 0u;
        }
    } else {
        /* USB buffer underrun / no stream: stop coil current completely. */
        P3_0 = 0;
        pdm_level = 128u;
        pdm_muted = 1u;
        quiet_run = QUIET_SAMPLES;
        pdm_accum = 0u;
        playing = 0u;
    }
}

/* Called at 32 kHz from the UNCHANGED v4 Timer2. Keep it small. */
void audio_tick_32khz(void)
{
    uint8_t old_accum;

    if (pdm_phase == 0u) audio_next_sample();

    if (pdm_muted) {
        P3_0 = 0;
    } else {
        /* First-order 8-bit error-feedback pulse-density modulator.
         * Bit density approaches pcm_level/256 over multiple subcycles.
         */
        old_accum = pdm_accum;
        pdm_accum = (uint8_t)(pdm_accum + pdm_level);
        P3_0 = (pdm_accum < old_accum) ? 1 : 0;
    }

    if (++pdm_phase == 4u) pdm_phase = 0u;
}
