"""Ensure no hard draw-count limiter can drop game draws."""
from pathlib import Path
cpp = Path(__file__).resolve().parents[1] / 'app/src/main/cpp'
a = (cpp / 'd3d11_gles/d3d11_gles_a.inc').read_text()
b = (cpp / 'd3d11_gles/d3d11_gles_b.inc').read_text()
game = (cpp / 'android/android_game.c').read_text()
assert 'draw_limit_skip' not in a + b
assert 'draw_limit_record' not in a + b
assert 'MERCENARIES_DRAW_LIMIT_10S' not in a + game
assert 'glDrawArrays(' in b and 'glDrawElements(' in b
print('draw-count limit removed from Android game rendering')
