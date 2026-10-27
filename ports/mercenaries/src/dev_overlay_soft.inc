/* Software-rendered frame-time overlay for Android (no GDI). Same layout and data as the Windows version:
 * FPS, frame time, time-scale line and the last 160 frame intervals as a graph, composited by the renderer. */
#include <stdlib.h>
static int enabled;
static uint32_t pixels[360 * 128];
static ULONGLONG updated;
#define W 360
#define H 128

/* 3x5 glyphs, 3 bits per row, top row first. Upper case only. */
typedef struct { char c; unsigned char row[5]; } Glyph;
static const Glyph k_font[] = {
 {'A',{2,5,7,5,5}},{'B',{6,5,6,5,6}},{'C',{3,4,4,4,3}},{'D',{6,5,5,5,6}},{'E',{7,4,6,4,7}},{'F',{7,4,6,4,4}},{'G',{3,4,5,5,3}},
 {'H',{5,5,7,5,5}},{'I',{7,2,2,2,7}},{'J',{1,1,1,5,2}},{'K',{5,5,6,5,5}},{'L',{4,4,4,4,7}},{'M',{5,7,7,5,5}},{'N',{6,5,5,5,5}},
 {'O',{2,5,5,5,2}},{'P',{6,5,6,4,4}},{'Q',{2,5,5,7,3}},{'R',{6,5,6,5,5}},{'S',{3,4,2,1,6}},{'T',{7,2,2,2,2}},{'U',{5,5,5,5,7}},
 {'V',{5,5,5,5,2}},{'W',{5,5,7,7,5}},{'X',{5,5,2,5,5}},{'Y',{5,5,2,2,2}},{'Z',{7,1,2,4,7}},
 {'0',{7,5,5,5,7}},{'1',{2,6,2,2,7}},{'2',{6,1,2,4,7}},{'3',{6,1,2,1,6}},{'4',{5,5,7,1,1}},{'5',{7,4,6,1,6}},{'6',{3,4,7,5,7}},
 {'7',{7,1,2,2,2}},{'8',{7,5,7,5,7}},{'9',{7,5,7,1,6}},{'.',{0,0,0,0,2}},{'%',{5,1,2,4,5}},{'|',{2,2,2,2,2}},{'/',{1,1,2,4,4}},{'-',{0,0,7,0,0}},
};
static void put(int x, int y, unsigned rgb)
{
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    unsigned r = (rgb >> 16) & 255u, g = (rgb >> 8) & 255u, b = rgb & 255u, a = r > g ? r : g; if (b > a) a = b;
    pixels[y * W + x] = (rgb & 0xffffffu) | (a << 24);                     /* premultiplied, like the GDI conversion */
}
static void text(int x, int y, const char *s, unsigned rgb)
{
    const int scale = 3;
    for (; *s; ++s, x += 4 * scale) {
        char c = (char)((*s >= 'a' && *s <= 'z') ? *s - 32 : *s);
        for (size_t g = 0; g < sizeof(k_font) / sizeof(k_font[0]); ++g) {
            if (k_font[g].c != c) continue;
            for (int r = 0; r < 5; ++r) for (int col = 0; col < 3; ++col)
                if (k_font[g].row[r] & (4 >> col))
                    for (int dy = 0; dy < scale; ++dy) for (int dx = 0; dx < scale; ++dx) put(x + col * scale + dx, y + r * scale + dy, rgb);
            break;
        }
    }
}
static void line(int x0, int y0, int x1, int y1, unsigned rgb)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        put(x0, y0, rgb); if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err; if (e2 >= dy) { err += dy; x0 += sx; } if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
void recomp_dev_overlay_set(int value) { enabled = value && (recomp_dev_menu_allowed() || getenv("MERCENARIES_FPS_OVERLAY")); updated = 0; }
static void paint(void)
{
    if (!enabled) return;
    ULONGLONG now = GetTickCount64();
    if (!updated || now - updated >= 100) {
        uint32_t times[160]; uint64_t serial; unsigned count = d3d8_GetFrameIntervalHistory(0, times, 160, &serial);
        memset(pixels, 0, sizeof(pixels));
        double sum = 0; unsigned recent = count < 60 ? count : 60;
        for (unsigned i = count - recent; i < count; ++i) sum += times[i];
        char buf[128];
        snprintf(buf, sizeof(buf), "FPS %.1f  %.2f MS", sum ? recent * 1000000. / sum : 0, count ? times[count - 1] / 1000. : 0);
        text(10, 6, buf, 0xE6F4F6);
        snprintf(buf, sizeof(buf), "WORLD %s", recomp_freecam_time_scale() == 0 ? "FROZEN" : recomp_freecam_time_scale() < 1 ? "20%" : "100%");
        text(10, 26, buf, 0xA4BBC3);
        for (int y = 80; y <= 100; y += 20) line(10, y, 350, y, 0x414F56);   /* 33.3 / 16.7 ms guides */
        for (unsigned i = 0, px = 0, py = 0; i < count; ++i) {
            int y = 120 - (int)(times[i] * .0012); if (y < 48) y = 48; int x = 10 + (int)i * 340 / 159;
            if (i) line((int)px, (int)py, x, y, 0x63D3DB);
            px = (unsigned)x; py = (unsigned)y;
        }
        /* dark one-pixel outline around coloured pixels, as in the Windows version */
        static uint32_t copy[W * H]; memcpy(copy, pixels, sizeof(pixels));
        for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
            if (copy[y * W + x] & 0xffffffu) continue; int near = 0;
            for (int dy = -1; dy <= 1 && !near; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                int nx = x + dx, ny = y + dy; if (nx >= 0 && nx < W && ny >= 0 && ny < H && (copy[ny * W + nx] & 0xffffffu)) { near = 1; break; } }
            if (near) pixels[y * W + x] = 192u << 24;
        }
        updated = now;
    }
    d3d8_CompositeHostOverlay(pixels, W, H, 12, 12, 960, 540);
}
void recomp_dev_overlay_init(void)
{
    d3d8_SetHostOverlayCallback(paint);
    if (getenv("MERCENARIES_FPS_OVERLAY")) { enabled = 1; updated = 0; }   /* performance monitoring on devices */
}
