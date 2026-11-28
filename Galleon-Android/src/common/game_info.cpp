#include <algorithm>
#include <atomic>
#include <iterator>

#include <toml++/toml.hpp>

#include "app/app.h"
#include "audio/output.h"
#include "common/menu.h"
#include "common/mouse_keyboard.h"
#include "common/options.h"
#include "core/log.h"

namespace game {

namespace {

// port 0 has the touch controls, the keyboard and mouse; the game sees no input while the panel is open
void Pad(int port, input::Pad& pad) {
    input::SeenPad(pad);
    if (port == 0) pad.connected = true;
    if (app::MenuOpen()) {
        // GALLEON-INPUT-FIXES: while the panel is open the game is blind: log when it swallows held input
        if (pad.buttons || pad.analog[0] || pad.analog[1]) {
            static std::atomic<int> n{0};
            if (n.fetch_add(1) < 20) LOGR("input: panel open, held input swallowed btn %04x", pad.buttons);
        }
        pad = input::Pad{.connected = pad.connected};
    } else if (port == 0) input::Keyboard(pad);
}

// no vibration while the keyboard and mouse are in use
void Rumble(int port, uint16_t& left, uint16_t& right) {
    if (port != 0 || !input::KeyboardActive()) return;
    left = 0;
    right = 0;
}

// what the options list cannot hold
void Load(const void* table) {
    const toml::table& t = *static_cast<const toml::table*>(table);
    Options& o = Opt();
    o.mouse_sensitivity = float(t["mouse_sensitivity"].value_or(double(o.mouse_sensitivity)));
    // the touch screen followed the mouse's before it had its own: a player's choice carries over
    o.touch_sensitivity = float(t["touch_sensitivity"].value_or(double(o.mouse_sensitivity)));
    o.left_sensitivity = float(t["left_stick_sensitivity"].value_or(double(o.left_sensitivity)));
    o.right_sensitivity = float(t["right_stick_sensitivity"].value_or(double(o.right_sensitivity)));
    std::string language = t["language"].value_or(o.language);
    bool known = std::find(std::begin(kLanguages), std::end(kLanguages), language) != std::end(kLanguages);
    if (known) o.language = language;
    if (const toml::table* keys = t["keys"].as_table())
        for (auto&& [id, value] : *keys)
            if (auto v = value.value<std::string>()) o.keys[std::string(id.str())] = *v;
}

// after the settings load, before boot
void Init() {
    BootLanguage();  // latched before the options can change the setting
    input::LoadBindings();
    audio::SetOutputMode(Opt().audio_output);
}

void Save(void* table) {
    toml::table& t = *static_cast<toml::table*>(table);
    const Options& o = Opt();
    t.insert("mouse_sensitivity", double(o.mouse_sensitivity));
    t.insert("touch_sensitivity", double(o.touch_sensitivity));
    t.insert("left_stick_sensitivity", double(o.left_sensitivity));
    t.insert("right_stick_sensitivity", double(o.right_sensitivity));
    t.insert("language", o.language);
    toml::table keys;
    for (const auto& [id, value] : o.keys) keys.insert(id, value);
    t.insert("keys", keys);
}

}  // namespace

Options& Opt() {
    static Options o;
    return o;
}

const std::string& BootLanguage() {
    static const std::string language = Opt().language;
    return language;
}

}  // namespace game

const app::GameInfo app::kGame = {
    .title = L"Galleon",
    .short_title = L"Galleon",
    .name = L"Galleon",
    .log = "galleon",
    .settings = "galleon.toml",
    .splash = nullptr,
    .missing = L"Put the game in the Galleon folder on the phone's storage: its disc image (.iso), or default.xbe with "
               L"the folders from the disc beside it.",
    .images = {{"default.xbe", "Galleon"}},
    .options = {{"screen_filter", nullptr, &game::Opt().screen_filter},
                {"bloom", &game::Opt().bloom, nullptr},
                {"fog", &game::Opt().fog, nullptr},
                {"anti_aliasing", nullptr, &game::Opt().anti_aliasing},
                {"hdr", &game::Opt().hdr, nullptr},
                {"invert_mouse", &game::Opt().invert_mouse, nullptr},
                {"hold_aim", &game::Opt().hold_aim, nullptr},
                {"walk_speed", nullptr, &game::Opt().walk_speed},
                {"left_stick_deadzone", nullptr, &game::Opt().left_deadzone},
                {"right_stick_deadzone", nullptr, &game::Opt().right_deadzone},
                {"dynamic_resolution", &game::Opt().dynamic_resolution, nullptr},
                {"dynamic_target_fps", nullptr, &game::Opt().dynamic_target_fps},
                {"dynamic_min_percent", nullptr, &game::Opt().dynamic_min_percent},
                {"dynamic_max_percent", nullptr, &game::Opt().dynamic_max_percent},
                {"dynamic_hold", nullptr, &game::Opt().dynamic_hold},
                {"audio_output", nullptr, &game::Opt().audio_output},
                {"debug_render_mode", nullptr, &game::Opt().debug_render_mode}},
    .load = game::Load,
    .save = game::Save,
    .init = game::Init,
    .loaded = nullptr,
    .window = nullptr,
    .message = nullptr,
    .ui = app::DrawUi,
    .pad = game::Pad,
    .rumble = game::Rumble,
};
