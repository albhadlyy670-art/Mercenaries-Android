// Galleon: what the host needs from this file. Black's game code was full of guest-address hooks (FOV, HUD, cheats,
// weapons, the in-game options menu); none of those addresses survive the hop to Galleon's image, so the features
// they implemented are absent, not emulated with Black's numbers. What remains is host-side.
#include <string>

#include <algorithm>
#include <iterator>

#include "common/options.h"

namespace game {

// Galleon's disc has not been checked for localized strings, so this build cannot tell a language the game has from
// one it does not: the LANGUAGE row offers the six text languages the port itself is translated into, and the game's
// own strings stay in English either way; only the panel's text follows it.
bool LanguageAvailable(const std::string& code) {
    return std::find(std::begin(kLanguages), std::end(kLanguages), code) != std::end(kLanguages);
}

}  // namespace game
