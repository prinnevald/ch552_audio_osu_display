/* SPDX-License-Identifier: MIT
 * Timer2: 32 kHz exact average rate from CH552 24 MHz CPU clock.
 * Timer2 input Fsys/12 = 2 MHz, hence 62.5 ticks per PDM bit.
 * Alternate reload counts 63 and 62: 2 MHz / 62.5 = 32,000 Hz.
 * 4 bit times = 1 PCM sample at 8 kHz; 32 bit times = 1 millisecond.
 * Preserves upstream tick_now()/tick_now16() for keyboard debouncing.
 */
#include <stdint.h>
#include "ch552.h"
#include "config.h"
#include "tick.h"
#include "audio.h"

#if FREQ_SYS != 24000000UL
#error "32-kHz PDM Timer2 configuration requires FREQ_SYS=24000000"
#endif

volatile uint32_t tick_ms = 0u;
static __data uint8_t pdm_ticks_in_ms = 0u;
static __data uint8_t reload_odd = 0u;

void tick_init(void)
{
    T2CON = 0x00u;
    T2MOD = 0x00u;    /* auto-reload, Fsys/12 */
    RCAP2H = 0xFFu;
    RCAP2L = 0xC1u;  /* 65536-63 */
    TH2 = RCAP2H;
    TL2 = RCAP2L;
    pdm_ticks_in_ms = 0u;
    reload_odd = 0u;
    TF2 = 0;
    ET2 = 1;
    TR2 = 1;
}

void tick_isr(void) __interrupt(INT_NO_TMR2)
{
    TF2 = 0;
    /* Reload is captured by hardware at overflow. Prepare the NEXT
     * interval in the alternating 63,62,63,62,... pattern. */
    RCAP2L = reload_odd ? 0xC1u : 0xC2u;
    reload_odd ^= 1u;

    audio_tick_32khz();

    if (++pdm_ticks_in_ms >= 32u) {
        pdm_ticks_in_ms = 0u;
        ++tick_ms;
    }
}

uint32_t tick_now(void)
{
    uint32_t value;
    uint8_t was_enabled = EA;
    EA = 0;
    value = tick_ms;
    EA = was_enabled;
    return value;
}

uint16_t tick_now16(void)
{
    uint16_t value;
    uint8_t was_enabled = EA;
    EA = 0;
    value = (uint16_t)tick_ms;
    EA = was_enabled;
    return value;
}
