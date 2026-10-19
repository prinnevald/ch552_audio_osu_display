#!/usr/bin/env python3
"""Install tested CH552 USB HID + 8 kHz PDM sources into tadakado/ch552-dongle.

Input must be a previously cloned upstream repository. Files outside the
explicitly listed paths are preserved; originals are backed up into a
non-source .ch552_audio_backup directory for easy recovery.
"""

import argparse
from pathlib import Path
import re
import shutil
import sys

ROOT = Path(__file__).resolve().parent.parent
REQUIRED = (
    'Makefile', 'config.h', 'src/cdc.c', 'src/hid.c',
    'src/sys.c', 'src/usb.c', 'src/usb_desc.c', 'src/main.c', 'src/tick.c',
)
SOURCES = ('main.c', 'audio.c', 'audio.h', 'tick.c')
SRCS_LINE = ('SRCS := $(addprefix src/,audio.c cdc.c hid.c main.c '
             'sys.c tick.c usb.c usb_desc.c)')


def install(repo: Path):
    if not repo.is_dir() or any(not (repo / item).is_file() for item in REQUIRED):
        sys.exit('Error: expected a clone of tadakado/ch552-dongle (missing original source files).')

    makefile = repo / 'Makefile'
    config = repo / 'config.h'
    mk = makefile.read_text(encoding='utf-8')
    cfg = config.read_text(encoding='utf-8')
    updated_mk, changes_mk = re.subn(r'^SRCS\s*:=.*$',
                                      lambda match: SRCS_LINE, mk, flags=re.M)
    updated_cfg, changes_cfg = re.subn(
        r'(#define\s+USB_HID_INTERVAL_MS\s+)(?:10|1)\b',
        r'\g<1>1', cfg)
    if changes_mk != 1 or changes_cfg != 1:
        sys.exit('Error: upstream Makefile/config.h format changed; refusing to patch.')
    if not re.search(r'#define\s+FREQ_SYS\s+24000000(?:UL|L|U)?\b', cfg):
        sys.exit('Error: this Timer2 driver needs FREQ_SYS=24000000 (24 MHz).')

    backups = repo / '.ch552_audio_backup'
    backups.mkdir(exist_ok=True)
    targets = [repo / 'src' / name for name in SOURCES] + [makefile, config]
    for target in targets:
        if target.exists():
            backup = backups / target.relative_to(repo)
            backup.parent.mkdir(parents=True, exist_ok=True)
            if not backup.exists():
                shutil.copy2(target, backup)

    for name in SOURCES:
        shutil.copy2(ROOT / 'src' / name, repo / 'src' / name)
    makefile.write_text(updated_mk, encoding='utf-8')
    config.write_text(updated_cfg, encoding='utf-8')
    shutil.copy2(ROOT / 'README.md', repo / 'README_AUDIO_OVERLAY.md')
    ignore = repo / '.gitignore'
    existing_ignore = ignore.read_text(encoding='utf-8') if ignore.exists() else ''
    if '.ch552_audio_backup/' not in existing_ignore:
        ignore.write_text(existing_ignore.rstrip() + '\n\n# CH552 audio installer backups\n.ch552_audio_backup/\n', encoding='utf-8')
    (repo / 'host').mkdir(exist_ok=True)
    shutil.copy2(ROOT / 'host' / 'stream_audio.py', repo / 'host' / 'stream_audio.py')
    shutil.copy2(ROOT / 'host' / 'requirements.txt', repo / 'host' / 'requirements.txt')
    print('Installed CH552 HID + 8 kHz audio sources, README_AUDIO_OVERLAY.md and host streamer.')
    print('Original overwritten files saved in .ch552_audio_backup/ (first run only).')
    print('Upstream README.md, LICENSE, USB stack, and tools preserved.')
    print(f'Next: cd {repo} && make clean && make')
    print('After successful compile: enter CH552 bootloader, then ch55xtool -f build/fw.bin')
    print('Publish this overlay repo, not the patched upstream checkout.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('upstream_repository', type=Path,
                        help='Path to cloned tadakado/ch552-dongle')
    args = parser.parse_args()
    install(args.upstream_repository.expanduser().resolve())


if __name__ == '__main__':
    main()
