/* CH552 USB HID Z/X + CDC 8-kHz PDM buzzer audio
 * Drop-in replacement for src/main.c in tadakado/ch552-dongle.
 * MIT SPDX-Identifier: MIT
 */
#include <stdint.h>
#include "ch552.h"
#include "config.h"
#include "sys.h"
#include "tick.h"
#include "usb.h"
#include "hid.h"
#include "cdc.h"
#include "audio.h"

/* Switches: P3.1 -> GND (Z), P1.1 -> GND (X).
 * If physical key labels are reversed, change SWAP_KEYS to 1.
 */
#define SWAP_KEYS 0
#define DEBOUNCE_MS 2
#if SWAP_KEYS
#define HID_KEY_Z 0x1B
#define HID_KEY_X 0x1D
#else
#define HID_KEY_Z 0x1D
#define HID_KEY_X 0x1B
#endif

static uint8_t raw_mask = 0;
static uint8_t stable_mask = 0;
static uint8_t sent_mask = 0xFF; /* Force initial release report. */
static uint16_t z_changed = 0, x_changed = 0;

static void buttons_init(void)
{
    P3_1 = 1;
    P1_1 = 1;
    P3_MOD_OC |= 0x02;
    P1_MOD_OC |= 0x02;
    P3_DIR_PU |= 0x02;
    P1_DIR_PU |= 0x02;
}

static void buttons_scan(uint16_t now)
{
    uint8_t mask = 0;
    if (!P3_1) mask |= 1;
    if (!P1_1) mask |= 2;

    if ((mask ^ raw_mask) & 1) z_changed = now;
    if ((mask ^ raw_mask) & 2) x_changed = now;
    raw_mask = mask;

    if ((raw_mask ^ stable_mask) & 1) {
        if ((uint16_t)(now - z_changed) >= DEBOUNCE_MS)
            stable_mask ^= 1;
    }
    if ((raw_mask ^ stable_mask) & 2) {
        if ((uint16_t)(now - x_changed) >= DEBOUNCE_MS)
            stable_mask ^= 2;
    }
}

static void keyboard_service(void)
{
    uint8_t codes[6] = {0, 0, 0, 0, 0, 0};
    if (!usb_is_configured() || usb_is_suspended()) {
        sent_mask = 0xFF;
        return;
    }
    if (stable_mask == sent_mask) return;
    if (stable_mask & 1) codes[0] = HID_KEY_Z;
    if (stable_mask & 2) codes[(stable_mask & 1) ? 1 : 0] = HID_KEY_X;
    if (hid_keyboard_report(0, codes)) sent_mask = stable_mask;
}

void main(void)
{
    uint16_t last_scan = 0;
    sys_init();
    buttons_init();
    tick_init();
    audio_init();
    usb_init();
    EA = 1;
    sys_watchdog_init();

    for (;;) {
        int16_t sample;
        uint16_t now;
        sys_watchdog_feed();

        /* USB CDC receives unframed unsigned 8-bit mono PCM at 8k samples/s.
         * Backpressure happens naturally because cdc_getc isn't called
         * when the audio FIFO has no space. The USB OUT endpoint then NAKs.
         */
        while (audio_has_space()) {
            sample = cdc_getc();
            if (sample < 0) break;
            audio_push((uint8_t)sample);
        }

        now = tick_now16();
        if (now != last_scan) {
            last_scan = now;
            buttons_scan(now);
        }
        keyboard_service();
    }
}
