// The render resolution following the frame rate (issue #66): each second of play is measured, a second under the
// target lowers the resolution at once by what it would take to reach it, and the target held for the hold time raises
// it a step. A step that brings the drop straight back is not tried again for a while, so a frame rate held at its cap
// does not swing between two resolutions. Every change costs the renderer a wait and its targets made again
// (vulkan.cpp's ApplySettings), hence a second's measure at least between changes
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

#include "app/app.h"
#include "common/menu.h"
#include "common/options.h"
#include "host.h"

namespace dynres {

namespace {

using clock = std::chrono::steady_clock;

constexpr auto kWindow = std::chrono::seconds(1);            // one measure
constexpr auto kHitch = std::chrono::milliseconds(250);      // a frame this long is a load or a pause: no measure
constexpr auto kSettle = std::chrono::milliseconds(500);     // after a change, its own hitch is not measured
constexpr auto kCeilingFor = std::chrono::seconds(30);       // a failed step up is not tried again this long
constexpr uint32_t kStep = 5;                                // percent, as the RENDER RESOLUTION row steps
constexpr float kLow = 0.92f;   // of the target: under this the resolution goes down
constexpr float kHeld = 0.98f;  // and at this the target is held

std::atomic<uint32_t> g_percent{0};  // 0 until play starts it, then the resolution in use

// the vulkan thread's own
clock::time_point g_start, g_last, g_changed, g_ceiling_until;
uint32_t g_frames = 0;
uint32_t g_ceiling = 0;  // a step up to this or past it brought the drop back
bool g_raised = false;   // the last change was up

uint32_t Floor(uint32_t p) { return p / kStep * kStep; }

void Restart(clock::time_point now) {
    g_start = now;
    g_frames = 0;
}

void Change(uint32_t to, bool up, clock::time_point now) {
    g_percent = to;
    g_raised = up;
    g_changed = now;
    Restart(now + kSettle);
}

}  // namespace

uint32_t Percent() {
    const game::Options& o = game::Opt();
    uint32_t base = app::Get().render_percent;
    if (!o.dynamic_resolution) return base;
    uint32_t lo = std::min(o.dynamic_min_percent, o.dynamic_max_percent), hi = o.dynamic_max_percent;
    uint32_t p = g_percent;
    return std::clamp(p ? p : base, lo, hi);
}

void Frame() {
    const game::Options& o = game::Opt();
    auto now = clock::now();
    auto since = now - g_last;
    g_last = now;
    if (!o.dynamic_resolution) {
        g_percent = 0;
        return;
    }
    uint32_t p = Percent();
    if (!g_percent) {
        g_percent = p;
        g_changed = now;
        Restart(now);
    }
    // the panel open, or a frame that waited on a load or the background: no measure of the game's frame rate
    if (app::MenuOpen() || since > kHitch || now < g_start) {
        Restart(std::max(now, g_start));
        return;
    }
    g_frames++;
    auto el = now - g_start;
    if (el < kWindow) return;
    float fps = float(g_frames) / std::chrono::duration<float>(el).count();
    Restart(now);
    float target = float(std::max(o.dynamic_target_fps, 1u));
    uint32_t lo = std::min(o.dynamic_min_percent, o.dynamic_max_percent), hi = o.dynamic_max_percent;
    if (fps < target * kLow) {
        if (p <= lo) return;
        // the cost goes with the pixels, the square of the percent
        uint32_t to = Floor(uint32_t(float(p) * std::sqrt(fps / target)));
        to = std::clamp(std::min(to, p - kStep), lo, hi);
        // a step up that the drop followed straight away: that resolution is too much for now
        if (g_raised && now - g_changed < kWindow * 3) {
            g_ceiling = p;
            g_ceiling_until = now + kCeilingFor;
        }
        Change(to, false, now);
        return;
    }
    if (fps < target * kHeld || p >= hi) return;
    if (now - g_changed < std::chrono::seconds(std::max(o.dynamic_hold, 1u))) return;
    uint32_t to = std::min(p + kStep, hi);
    if (now < g_ceiling_until && to >= g_ceiling) return;
    Change(to, true, now);
}

}  // namespace dynres
