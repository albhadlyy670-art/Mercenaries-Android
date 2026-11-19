#!/usr/bin/env python3
"""Install the CPU tuning helper in the Android game thread (idempotent).

Run: python3 CPUFIX/apply_cpu_tuning.py [--repo-root PATH]
The repository already contains the installed version; this script can also
reapply it to an unmodified copy of android_game.c.
"""
import argparse
from pathlib import Path

HERE = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--repo-root', type=Path, default=HERE.parent)
args = parser.parse_args()
android = args.repo_root / 'ports/mercenaries-android/app/src/main/cpp/android'
game = android / 'android_game.c'
if not game.is_file():
    parser.error(f'android_game.c not found: {game}')
with game.open('r', encoding='utf-8', newline='') as f:
    text = f.read()
newline = '\r\n' if '\r\n' in text else '\n'

def insert_once(text, needle, insertion, marker, after=False):
    if marker in text:
        if text.count(marker) != 1:
            raise SystemExit(f'expected exactly one {marker}')
        return text
    if text.count(needle) != 1:
        raise SystemExit(f'expected exactly one anchor: {needle!r}')
    return text.replace(needle, needle + insertion if after else insertion + needle, 1)

text = insert_once(text, 'extern int mercenaries_android_main(void);' + newline,
                   '#include "android_cpu_tuning.inc"' + newline,
                   '#include "android_cpu_tuning.inc"', after=True)
text = insert_once(text, '    android_trace("[app] entering mercenaries_android_main");',
                   '    android_cpu_tuning_start(tmp_dir);' + newline,
                   'android_cpu_tuning_start(tmp_dir);')
text = insert_once(text, '    LOG("mercenaries_android_main returned %d", rc);',
                   '    android_cpu_tuning_stop();' + newline,
                   'android_cpu_tuning_stop();')
# Validate all anchors before touching either file.
helper = (HERE / 'android_cpu_tuning.inc').read_bytes()
(android / 'android_cpu_tuning.inc').write_bytes(helper)
with game.open('w', encoding='utf-8', newline='') as f:
    f.write(text)
print(f'CPU tuning installed in {game}')
