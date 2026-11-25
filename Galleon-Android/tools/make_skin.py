# The touch controls' button skins (issue #14, by bagasromadon) packed into one atlas for the APK:
#   python -I tools/make_skin.py <skin folder> app/src/main/assets/skins/buttons.png
# The folder is the one the skin came in, "With Button" (the Xbox letters) and "Without Button Annotation" (clean).
# Each image is scaled to the cell's width and sits at the bottom of its cell, so a button's circle is the cell's
# width across and ends at its bottom, the letter above it; the order is native/touch.cpp's SkinPart, the Xbox set
# first. Needs Pillow.
import os
import sys

from PIL import Image

CELL_W, CELL_H, COLUMNS = 192, 209, 6

# SkinPart order: the image of each in the Xbox set and in the clean one
PARTS = [
    ("fire", "R", "Fire"),
    ("aim", "right_click", "Aim"),
    ("mode", "up", "change_mode"),
    ("reload", "A", "Reload"),
    ("crouch", "left_click", "Crouch"),
    ("melee", "B", "Melee"),
    ("use", "X", "Action"),
    ("grenade", "L", "grenade"),
    ("weapon", "right", "change_weapon_up"),
    ("health", "down", "medic"),
    ("objectives", "back", "Back"),
    ("pause", "start", "Start"),
    ("gyro", "gyroscope", "Gyroscope"),
    ("back", "back", "Back"),
    ("suppressor", "Y", "Surpressor"),
    ("stick_base", "analog_base", "analog_base"),
    ("stick_knob", "analog_stick", "analog_stick"),
]
SETS = ["With Button", "Without Button Annotation"]


def main():
    src, out = sys.argv[1], sys.argv[2]
    count = len(SETS) * len(PARTS)
    rows = (count + COLUMNS - 1) // COLUMNS
    atlas = Image.new("RGBA", (CELL_W * COLUMNS, CELL_H * rows), (0, 0, 0, 0))
    for s, folder in enumerate(SETS):
        for p, part in enumerate(PARTS):
            im = Image.open(os.path.join(src, folder, part[1 + s] + ".png")).convert("RGBA")
            h = min(CELL_H, round(CELL_W * im.height / im.width))
            im = im.resize((CELL_W, h), Image.LANCZOS)
            i = s * len(PARTS) + p
            atlas.alpha_composite(im, ((i % COLUMNS) * CELL_W, (i // COLUMNS) * CELL_H + CELL_H - h))
    os.makedirs(os.path.dirname(out), exist_ok=True)
    atlas.save(out, optimize=True)
    print(f"{out}: {atlas.width}x{atlas.height}, {count} images")


if __name__ == "__main__":
    main()
