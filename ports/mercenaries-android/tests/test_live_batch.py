"""Ensure experimental merging cannot be enabled at runtime."""
from pathlib import Path
root = Path(__file__).resolve().parents[3]
core = (root / "src/nv2a/nv2a_core.c").read_text()
renderer = (root / "src/nv2a/nv2a_pgraph_d3d11.c").read_text()
game = (root / "ports/mercenaries-android/app/src/main/cpp/android/android_game.c").read_text()
assert "pgraph_d3d11_try_batch_inline_triangles" not in core + renderer
assert "nv2a_batch_inline_packet(" not in core
assert "MERCENARIES_TEST_NV2A_BATCH" not in core + game
assert "experimental draw merging disabled" in game
print("NV2A experimental draw merging disabled")
