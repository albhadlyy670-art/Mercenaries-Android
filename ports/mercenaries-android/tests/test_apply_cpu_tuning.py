"""The installer is repeatable, preserves CRLF, and never partially edits on failure."""
from pathlib import Path
import subprocess, sys, tempfile
root = Path(__file__).resolve().parents[3]
script = root / 'CPUFIX/apply_cpu_tuning.py'
source = root / 'ports/mercenaries-android/app/src/main/cpp/android/android_game.c'
helper = root / 'ports/mercenaries-android/app/src/main/cpp/android/android_cpu_tuning.inc'
assert helper.read_bytes() == (root / 'CPUFIX/android_cpu_tuning.inc').read_bytes()
s = source.read_text()
for marker in ['#include "android_cpu_tuning.inc"','android_cpu_tuning_start(tmp_dir);','android_cpu_tuning_stop();']:
    assert s.count(marker)==1
with tempfile.TemporaryDirectory() as tmp:
    d=Path(tmp)/'ports/mercenaries-android/app/src/main/cpp/android'; d.mkdir(parents=True)
    g=d/'android_game.c'
    original=('extern int mercenaries_android_main(void);\r\n'
              '    android_trace("[app] entering mercenaries_android_main");\r\n'
              '    LOG("mercenaries_android_main returned %d", rc);\r\n').encode()
    g.write_bytes(original)
    cmd=[sys.executable,str(script),'--repo-root',tmp]
    subprocess.run(cmd,check=True,stdout=subprocess.DEVNULL)
    first=g.read_bytes(); assert b'\r\n' in first and b'\n' not in first.replace(b'\r\n',b'')
    subprocess.run(cmd,check=True,stdout=subprocess.DEVNULL)
    assert first==g.read_bytes() and (d/'android_cpu_tuning.inc').read_bytes()==helper.read_bytes()
    g.write_bytes(b'bad anchor\n'); (d/'android_cpu_tuning.inc').write_bytes(b'unchanged')
    assert subprocess.run(cmd,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode != 0
    assert g.read_bytes()==b'bad anchor\n' and (d/'android_cpu_tuning.inc').read_bytes()==b'unchanged'
print('CPU tuning installer: idempotent, CRLF and failed-anchor safety OK')
