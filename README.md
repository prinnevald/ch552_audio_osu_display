# CH552 USB Keypad + Lo-Fi Audio Player

**A small source-code overlay for [tadakado/ch552-dongle](https://github.com/tadakado/ch552-dongle).** Adds a two-key osu! USB keyboard and an 8 kHz USB-controlled audio player, running simultaneously on one CH552. You do **not** need to fork or republish the upstream USB code.

Two mechanical switches send **Z** and **X** as ordinary USB HID keyboard keys. Independently, a Linux PC streams audio through the same USB cable as raw **8 kHz / 8-bit / mono PCM** over a **USB CDC virtual serial port**. A CH552 Timer2 interrupt converts these samples into a **32 kHz, 1-bit pulse-density modulated (PDM)** signal that drives a small electromagnetic buzzer through an NPN transistor.

Speech is intelligible but deliberately very tinny — think *bad pocket radio*. The `bright-wide` audio profile was preferred during real testing on this buzzer and is the **default**.

> **Status:** The 8 kHz PDM firmware and `bright-wide` profile were tested on a 5 V CH552 board with a 17 Ω electromagnetic buzzer; USB keyboard and audio playback worked. This overlay contains our additions/replacements only. **By itself it does not compile**: clone the upstream project and apply the overlay using the steps below. A fresh combined build has not been independently verified on every board.
>
> **Attribution:** The underlying USB CDC/HID firmware comes from [Tadashi (`tadakado`)](https://github.com/tadakado/ch552-dongle), under MIT. The original copyright line is **Copyright (c) 2026 Tadashi**. This repository includes the unaltered MIT license and [third-party notices](THIRD_PARTY_NOTICES.md).

## Features

- Simultaneous **USB HID keyboard** and **USB CDC-ACM** serial audio over one USB cable
- Independent buttons: press/release Z and X, including both at once
- **8 kHz**, unsigned **8-bit PCM**, mono input; **32 kHz** 1-bit PDM output
- No conventional PWM peripheral or Timer0; Timer2 drives the modulator and 1 ms debouncing clock
- A small FIFO (192 bytes, 191 usable) for streaming with backpressure
- Mutes output during sustained digital silence (important for an unfiltered buzzer)
- Built-in two-beep startup self-test
- PC audio conversion from WAV, MP3, OGG, etc., using FFmpeg
- `bright-wide` profile selected by default; other profiles available
- No LCD needed. The future 1602 LCD pin assignment is reserved, not implemented

## Wiring and parts

### CH552 GPIO assignment

| Function | CH552 pin | Connection |
|---|---|---|
| Keyboard switch Z | **P3.1** | Normally-open switch from P3.1 to GND |
| Keyboard switch X | **P1.1** | Normally-open switch from P1.1 to GND |
| Buzzer drive | **P3.0** | Through 1 kΩ to BC337 base (not to the buzzer directly) |
| USB | USB D+ / D- | Handled by the CH552 board and USB stack |

Buttons use the internal pull-ups; no external resistors are needed for the switches. Some CH552 boards have other hardware attached to P1.1, so check your **actual board** before wiring.

### Buzzer driver

**The tested buzzer is an electromagnetic 16–17 Ω passive buzzer, not a high-impedance piezo disk. Never connect this buzzer directly to a CH552 GPIO.**

```text
       CH552 5 V
           |
         [100 Ω]                 diode across buzzer
           |                    cathode = striped end
           +-----------|<|-----------+
           |           1N4007        |
       BUZZER (+)                   |
       BUZZER (-)-------------------+
           |
        Collector (C)
CH552      |            BC337 NPN
P3.0 --[1 kΩ]-- Base (B)
                        Emitter (E) --- GND

CH552 GND and driver GND are common.
```

**Wiring in words:** +5 V → 100 Ω series resistor → buzzer positive. Buzzer negative → BC337 collector. BC337 emitter → ground. P3.0 → 1 kΩ → BC337 base. 1N4007 **across the buzzer** (cathode/stripe to buzzer positive, anode to buzzer negative). Verify the **exact BC337 pinout** for your particular transistor; do not assume its physical lead order from the diagram. A 100 Ω series resistor limits the coil's steady-state current to roughly 40 mA. Use a ≥¼ W resistor.

The buzzer is optimized for signaling near 2 kHz, not high-fidelity sound. The transistor, flyback diode, current limiting and mechanical resonance all shape/distort the result.

### Pins reserved for a possible LCD1602

The LCD **is not supported by this firmware yet**. A possible future 4-bit HD44780 wiring plan is: RS=P3.2, E=P3.3, D4=P1.5, D5=P1.6, D6=P1.7, D7=P3.4; LCD RW=GND. Verify that your CH552 package/board exposes those pins. Its backlight may need a separate resistor and its contrast pin a potentiometer. The 5 V supply and LCD input logic must be compatible.

## Getting started (Debian / Ubuntu) — overlay workflow

This repository intentionally **does not duplicate** the upstream USB, HID and CDC firmware. You clone **two repositories**: the original by Tadashi, and this independent overlay. The installer then copies just four modified source files, updates two build settings, and adds the audio streamer and an English guide to your **local** upstream checkout. No fork is required.

### 1. Install dependencies

```bash
sudo apt update
sudo apt install git make sdcc python3 python3-serial ffmpeg
```

For flashing, install [`ch55xtool`](https://pypi.org/project/ch55xtool/) separately if necessary, or continue using your already-working CH552 flashing method.

### 2. Clone both repositories

```bash
# Original project (USB CDC/HID stack and build system)
git clone https://github.com/tadakado/ch552-dongle.git

# This overlay (replace with the actual URL of your GitHub project)
git clone https://github.com/prinnevald/ch552_audio_osu_display.git CH552-USB-Keypad-Audio
```

If you downloaded this overlay as a ZIP instead of cloning it, simply extract it as `CH552-USB-Keypad-Audio/` alongside `ch552-dongle/`.

### 3. Apply this overlay

From the parent directory containing both folders:

```bash
python3 CH552-USB-Keypad-Audio/tools/install_into_upstream.py ch552-dongle
cd ch552-dongle
make clean && make
```

The installer checks that the upstream source tree has the expected layout and is configured for 24 MHz. It patches `src/main.c`, `src/audio.c`, `src/audio.h`, `src/tick.c`, `Makefile`, and `config.h`; copies `host/stream_audio.py`; and adds `README_AUDIO_OVERLAY.md` with the English instructions. It **does not replace the original upstream README or LICENSE**. Original files it replaces are backed up in `.ch552_audio_backup/` (on the first run).

The original `USB_HID_INTERVAL_MS` value is changed from 10 ms to 1 ms. Upstream changes may require updating the installer; it refuses to apply unsupported Makefile/config layouts rather than guessing.

A successful build produces `build/fw.bin`. Check the compiler memory report (the CH552's available flash and RAM are very limited). Do not flash a failed build.

### 4. Flash the firmware

Enter the CH552 USB bootloader using the procedure for your board, then (if you use `ch55xtool`):

```bash
ch55xtool -f build/fw.bin
```

Reconnect the board normally after flashing. You should hear **two 500 Hz startup beeps**. The computer should enumerate a USB keyboard and a CDC-ACM serial interface. This is not a USB Audio Class speaker: audio **must** be sent using the Python program below. You cannot select it as a normal system-wide audio output without different USB firmware.

> Avoid `make flash` from the original upstream tree: its default `timing` target validates additional LED routines not used by this project. Compile with `make`, then use your CH552 flashing command separately.

## Using the Z/X keyboard

Plug the board in and press the two switches. They send normal USB key presses with approximately 2 ms non-blocking debouncing. Both can be pressed at once. In osu!, map the keys to Z and X if necessary. Audio playback does **not** trigger either switch, and switch presses do **not** trigger the buzzer.

If your physical switch labels are reversed, edit in `src/main.c`:

```c
#define SWAP_KEYS 1   /* instead of 0 */
```

Then rebuild and reflash. This only swaps the two HID key assignments, not the wiring.

## Playing audio from a Linux computer

Find your CDC serial port:

```bash
ls -l /dev/ttyACM*
```

If permission is denied, add yourself to Debian's serial group and log out/in:

```bash
sudo usermod -aG dialout "$USER"
```

**Default — preferred `bright-wide` profile:**

```bash
python3 host/stream_audio.py /dev/ttyACM0 speech.wav
python3 host/stream_audio.py /dev/ttyACM0 "Never Gonna Give You Up.mp3"
```

The program uses FFmpeg to decode the file, apply the chosen filter, downsample to 8 kHz, and emit unsigned 8-bit PCM. It sends the samples in small chunks at about real-time speed to prevent the CH552's tiny audio FIFO from overflowing.

Pick another profile or adjust gain:

```bash
python3 host/stream_audio.py /dev/ttyACM0 speech.wav --profile bright
python3 host/stream_audio.py /dev/ttyACM0 song.mp3 --profile music
python3 host/stream_audio.py /dev/ttyACM0 speech.wav --profile bright-wide --gain 1.3
```

Available profiles: `bright-wide` (**default**), `bright`, `voice`, `warm`, `music`, `raw`. The default noise gate helps the buzzer stop rattling between phrases. Tune it with `--gate 0` (disable) or `--gate-hold 100` (milliseconds).

### Preview the processed audio on your PC

```bash
python3 host/stream_audio.py preview speech.wav --save /tmp/speech8k.wav
ffplay -autoexit /tmp/speech8k.wav
```

### Generate a known test tone

```bash
ffmpeg -f lavfi -i 'sine=frequency=500:duration=3' \
  -ac 1 -ar 8000 -y /tmp/500hz.wav
python3 host/stream_audio.py /dev/ttyACM0 /tmp/500hz.wav --profile raw
```

### Troubleshooting

| Symptom | What to check |
|---|---|
| Keyboard works, no startup beeps | Recheck P3.0/driver and ensure `src/audio.c` **and** `src/tick.c` are the 32 kHz PDM versions |
| Beeps work but audio silent | Correct `/dev/ttyACM*`, Python serial permissions, host app and CDC enumeration |
| Audio is half-speed | You flashed 4 kHz firmware but are sending 8 kHz audio; rebuild/reflash this v6 version |
| Noise during silence | Use the host noise gate and the PDM driver with 4 ms quiet gating; do not revert to old PWM firmware |
| Key labels reversed | Set `SWAP_KEYS=1` in `src/main.c`, rebuild and reflash |
| Crackles/stutters | Avoid USB hubs, close programs using the serial port, try smaller chunks (`--chunk 16`) or lower gain |
| Build out of RAM | Send the compiler memory report; do not flash a failed build |

## Architecture and limitations

```text
Linux PC
  audio file --FFmpeg/filter/8k PCM--> USB CDC-ACM
                                      |
CH552 (24 MHz)                       FIFO, ~24 ms
  USB HID keyboard <----- Z/X ----   | 
  PDM accumulator <--- Timer2 32 kHz <+ 4 PDM bits/sample
             |
             P3.0 --> 1 kΩ --> BC337 --> 17 Ω magnetic buzzer
```

At 8 kHz, the Nyquist limit is 4 kHz; the 32 kHz PDM signal provides only 4 modulator ticks for each sample. The PDM carrier, the coil, flyback diode and buzzer's resonant response all contribute to the deliberately tinny audio. **There is no analog reconstruction filter or true audio amplifier.** This is a fun experiment rather than a production speaker.

The working firmware uses Timer2 and does **not** drive the buzzer from the buttons. It intentionally disables Timer0 and hardware PWM routing and keeps P3.0 as GPIO. Do not combine it with the earlier Arduino tone example or the v3 8-bit PWM experiments.

Potential future work: multi-band audio using different transducers/driver circuits, 1602 status display, low-latency HID characterization, improved modulation, and audio-driver experiments. Multiple identical buzzers alone are unlikely to extend the frequency response.

## Files

In this overlay package:

```text
CH552-USB-Keypad-Audio/
├── README.md
├── LICENSE
├── .gitignore
├── src/
│   ├── main.c                  # USB HID Z/X + USB CDC PCM main loop
│   ├── audio.c                 # 8 kHz FIFO, muting, 32 kHz PDM output
│   ├── audio.h
│   └── tick.c                  # Timer2 32 kHz + 1 ms counter
├── host/
│   ├── stream_audio.py        # PC-side FFmpeg/Python streamer
│   └── requirements.txt
└── tools/
    └── install_into_upstream.py
```

After running the installer, your *local* `ch552-dongle` directory has the upstream USB stack and our modified sources. **Publish this overlay repository itself**, not the resulting patched checkout. Users follow the instructions above to reconstruct the build on their own machines. Keep the upstream repository separate and linked; there is no need to re-upload its code or make a GitHub fork.

## Attribution and licenses

**Original CH552 composite USB firmware:** [Tadashi (`tadakado`), ch552-dongle](https://github.com/tadakado/ch552-dongle), MIT license, **Copyright (c) 2026 Tadashi**. The upstream USB CDC/HID stack, headers, and build infrastructure remain in the user's separately cloned upstream repository.

**This overlay:** independent CH552 osu! keyboard, PDM audio and Linux audio streamer additions/replacements, distributed under the MIT license with the original upstream copyright notice retained. Some replacement source files adapt the upstream firmware interfaces. See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md). When distributing the combined firmware or derived copies, preserve the upstream copyright/license notices.

**Why not fork?** You may fork the original repository instead; that is normal for MIT-licensed work and GitHub will show its provenance. We use the overlay layout to minimize duplicated code and make your additions easy to review. It requires cloning both repositories before building.
