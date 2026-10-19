# Upstream attribution and third-party notices

The [tadakado/ch552-dongle](https://github.com/tadakado/ch552-dongle)
project, authored by **Tadashi** (`tadakado`), provides the underlying CH552
USB composite device firmware (CDC-ACM and HID), hardware initialization,
headers, and build system on which this overlay is based.

**Original copyright notice:** `Copyright (c) 2026 Tadashi`.

**License:** MIT. A verbatim copy of the upstream `LICENSE` is included in
this overlay as a courtesy for license compliance and attribution. The
installer leaves the upstream repository's LICENSE intact.

The overlay provides replacement/adapted firmware for the two-button keyboard
and 8 kHz / 32 kHz PDM audio output, and a Python/FFmpeg streamer with
`bright-wide` filtering. It does not redistribute the complete upstream
USB stack. Some replacement source files use upstream interfaces and may
constitute derivative work under the upstream MIT license.

Please also consult the [upstream README](https://github.com/tadakado/ch552-dongle#readme)
for the original author's acknowledgments and any additional referenced work.
