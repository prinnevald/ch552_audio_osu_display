#!/usr/bin/env python3
"""CH552 composite HID+CDC 8 kHz audio streamer (for v6 8-kHz PDM firmware).

PC-side speech shaping; REQUIRES the matching CH552 v6 8-kHz firmware.
Audio stays unsigned 8-bit mono PCM at 8,000 samples/s.
Requires ffmpeg; requires pyserial only when sending to a real serial port.
"""

import argparse
import math
import shutil
import subprocess
import sys
import time
import wave
from pathlib import Path

RATE = 8000

# Filters are applied BEFORE downsampling to 8 kHz (Nyquist = 4 kHz).
# Boosts are deliberately moderate: a magnetic buzzer is extremely resonant.
PROFILES = {
    "raw": "anull",
    "voice": (
        "highpass=f=260,lowpass=f=1700,"
        "equalizer=f=1050:t=q:w=1.0:g=4,"
        "acompressor=threshold=0.13:ratio=3:attack=5:release=90:makeup=2,"
        "alimiter=limit=0.92"
    ),
    "bright": (
        "highpass=f=420,lowpass=f=1820,"
        "equalizer=f=1400:t=q:w=0.9:g=7,"
        "acompressor=threshold=0.12:ratio=3:attack=5:release=75:makeup=2,"
        "alimiter=limit=0.92"
    ),
    "bright-wide": (
        "highpass=f=420,lowpass=f=3350,"
        "equalizer=f=1400:t=q:w=0.9:g=5,"
        "equalizer=f=2600:t=q:w=1.0:g=5,"
        "acompressor=threshold=0.12:ratio=3:attack=5:release=75:makeup=2,"
        "alimiter=limit=0.92"
    ),
    "warm": (
        "highpass=f=180,lowpass=f=1250,"
        "equalizer=f=700:t=q:w=1:g=4,"
        "acompressor=threshold=0.12:ratio=3:attack=7:release=95:makeup=2,"
        "alimiter=limit=0.92"
    ),
    "music": (
        "highpass=f=180,lowpass=f=1700,"
        "acompressor=threshold=0.18:ratio=2:attack=10:release=120:makeup=1.6,"
        "alimiter=limit=0.92"
    ),
}


def convert_audio(source: Path, profile: str) -> bytes:
    if not shutil.which("ffmpeg"):
        raise RuntimeError("FFmpeg is not installed (sudo apt install ffmpeg)")
    cmd = [
        "ffmpeg", "-hide_banner", "-loglevel", "error", "-nostdin", "-i", str(source),
        "-vn", "-ac", "1", "-af", PROFILES[profile],
        "-ar", str(RATE), "-acodec", "pcm_u8", "-f", "u8", "-",
    ]
    return subprocess.run(cmd, stdout=subprocess.PIPE, check=True).stdout


def adjust_gain(payload: bytes, gain: float) -> bytes:
    if gain == 1.0:
        return payload
    out = bytearray(len(payload))
    for i, value in enumerate(payload):
        centered = (value - 128) * gain
        out[i] = max(0, min(255, int(round(centered + 128))))
    return bytes(out)


def noise_gate(payload: bytes, threshold: float, hold_ms: int) -> bytes:
    """Simple 20-ms frame gate, with a hold time for trailing consonants.

    The CH552 firmware gates a true silence value of 128. Frames below
    an RMS threshold are replaced with precisely 128, instead of adding
    a high-frequency dithering hiss during pauses. Disable using --gate 0.
    """
    if threshold <= 0:
        return payload
    frame_size = RATE // 50  # 20 ms
    hold_frames = (hold_ms + 19) // 20
    remaining = 0
    result = bytearray(payload)
    for start in range(0, len(result), frame_size):
        stop = min(len(result), start + frame_size)
        total = sum((int(s) - 128) ** 2 for s in result[start:stop])
        rms = math.sqrt(total / (stop - start))
        if rms >= threshold:
            remaining = hold_frames
        elif remaining > 0:
            remaining -= 1
        else:
            result[start:stop] = bytes([128]) * (stop - start)
    return bytes(result)


def write_preview(path: Path, payload: bytes):
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(1)
        output.setframerate(RATE)
        output.writeframes(payload)


def stream(port: str, payload: bytes, chunk: int, prebuffer: int):
    try:
        import serial
    except ImportError as ex:
        raise RuntimeError("PySerial missing (sudo apt install python3-serial)") from ex
    # v6 firmware has a 192-byte FIFO (191 usable), leaving space for
    # buffering at 8 kHz. Host timing jitter can still cause underruns.
    prebuffer = min(prebuffer, 160)
    prebuffer -= prebuffer % chunk
    prebuffer = min(prebuffer, len(payload))
    with serial.Serial(port, 115200, timeout=0.5, write_timeout=3) as ser:
        time.sleep(0.25)
        if prebuffer:
            ser.write(payload[:prebuffer])
        sent = prebuffer
        start = time.monotonic()
        while sent < len(payload):
            n = min(chunk, len(payload) - sent)
            # Wait a chunk time before refilling, keeping the first buffer
            # as jitter headroom. Each USB write is allowed to block on NAK.
            target = start + (sent - prebuffer + n) / RATE
            wait = target - time.monotonic()
            if wait > 0:
                time.sleep(wait)
            ser.write(payload[sent:sent + n])
            sent += n
        ser.flush()
    # Allow the buffered tail to play before the caller considers it finished.
    print("USB writes finished (last buffered samples may still be playing).")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="Serial device, e.g. /dev/ttyACM0, or 'preview'")
    parser.add_argument("file", type=Path, help="WAV, MP3, OGG, etc.")
    parser.add_argument("--profile", choices=PROFILES, default="bright-wide")
    parser.add_argument("--gain", type=float, default=1.0, help="PCM amplitude gain, default 1.0")
    parser.add_argument("--gate", type=float, default=2.5,
                        help="Noise gate threshold, RMS in 8-bit PCM units (0 disables)")
    parser.add_argument("--gate-hold", type=int, default=80, help="Gate hold after speech, ms")
    parser.add_argument("--chunk", type=int, choices=(16, 32, 64), default=32)
    parser.add_argument("--prebuffer", type=int, default=160, help="Initial bytes; max 160")
    parser.add_argument("--save", type=Path, help="Save the processed audio as a 8-kHz WAV")
    args = parser.parse_args()

    if not args.file.is_file():
        parser.error(f"Audio file not found: {args.file}")
    if not 0.1 <= args.gain <= 3.0:
        parser.error("--gain must be between 0.1 and 3")
    if args.gate < 0 or args.gate_hold < 0 or args.prebuffer < 0:
        parser.error("--gate, --gate-hold and --prebuffer cannot be negative")
    try:
        payload = convert_audio(args.file, args.profile)
        payload = adjust_gain(payload, args.gain)
        payload = noise_gate(payload, args.gate, args.gate_hold)
        if not payload:
            raise RuntimeError("Input contained no audio frames")
        print(f"Profile: {args.profile}; duration {len(payload) / RATE:.2f} s; "
              f"sample rate {RATE} Hz; quiet bytes "
              f"{payload.count(128) / len(payload) * 100:.1f}%")
        if args.save:
            write_preview(args.save, payload)
            print(f"Saved 8 kHz preview: {args.save}")
        if args.port.lower() != "preview":
            stream(args.port, payload, args.chunk, args.prebuffer)
        elif not args.save:
            print("Preview mode: specify --save /tmp/example.wav to generate a file.")
        return 0
    except (OSError, subprocess.CalledProcessError, RuntimeError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
