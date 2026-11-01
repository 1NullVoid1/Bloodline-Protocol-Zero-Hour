#include "GameData.h"
#include "raylib.h"
#include <fstream>
#include <sstream>
#include <string>

// ------------------------------------------------------------
// Saving your progress: player level, weapon levels and loadouts.
// It goes in profile.cfg next to game.exe, as plain text like:
//     playerLevel=4
//     weapon.0=7 350              (weapon 0 is level 7, with 350 XP toward level 8)
//     loadout.0.primary=0 1 -1 ...  (weapon, then the 7 attachment slots, then the camo)
// Delete the file to start over from level 1.
// ------------------------------------------------------------

static std::string profilePath() {
    return std::string(GetApplicationDirectory()) + "profile.cfg";
}

static void writeBuild(std::ofstream& file, const WeaponBuild& build) {
    file << build.weapon;
    for (int slot = 0; slot < SLOT_COUNT; slot++) file << " " << build.attachment[slot];
    file << " " << build.camo << "\n";
}

void saveProfile(const Profile& profile) {
    std::ofstream file(profilePath());
    if (!file) {
        TraceLog(LOG_WARNING, "Couldn't save your progress to %s", profilePath().c_str());
        return;
    }
    file << "playerLevel=" << profile.playerLevel << "\n";
    file << "playerXp=" << profile.playerXp << "\n";
    for (size_t i = 0; i < profile.weaponProgress.size(); i++) {
        file << "weapon." << i << "=" << profile.weaponProgress[i].level << " " << profile.weaponProgress[i].xp << "\n";
    }
    file << "loadouts=" << profile.loadouts.size() << "\n";
    for (size_t i = 0; i < profile.loadouts.size(); i++) {
        const Loadout& l = profile.loadouts[i];
        file << "loadout." << i << ".name=" << l.name << "\n";
        file << "loadout." << i << ".primary=";
        writeBuild(file, l.primary);
        file << "loadout." << i << ".secondary=";
        writeBuild(file, l.secondary);
        file << "loadout." << i << ".equipment=" << l.tactical << " " << l.lethal << "\n";
    }
}

// ------------------------------------------------------------
// Loading. Anything that doesn't make sense (a hand-edited file, or a weapon
// that was removed from the game) is skipped instead of crashing.
// ------------------------------------------------------------

static int clampInt(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

// Reads "weapon a0 a1 a2 a3 a4 a5 a6 camo" into a build. Returns false if it's no good.
static bool readBuild(const std::string& text, WeaponBuild& build, bool primary) {
    std::istringstream in(text);
    WeaponBuild b;
    if (!(in >> b.weapon)) return false;
    if (b.weapon < 0 || b.weapon >= (int)WEAPONS.size()) return false;
    if (WEAPONS[b.weapon].isPrimary != primary) return false;
    const WeaponDef& weapon = WEAPONS[b.weapon];
    for (int slot = 0; slot < SLOT_COUNT; slot++) {
        int index = -1;
        if (!(in >> index)) return false;
        // Only keep it if it's a real attachment for this slot
        bool valid = index >= 0 && index < (int)weapon.attachments.size() && weapon.attachments[index].slot == slot;
        b.attachment[slot] = valid ? index : -1;
    }
    if (!(in >> b.camo) || b.camo < 0 || b.camo >= (int)CAMOS.size()) b.camo = 0;
    if (b.attachmentCount() > MAX_ATTACHMENTS) return false;
    build = b;
    return true;
}

Profile loadProfile() {
    Profile profile = makeStartingProfile();
    std::ifstream file(profilePath());
    if (!file) {
        return profile;   // first launch: a fresh profile
    }

    std::vector<Loadout> loadouts;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();   // in case Notepad added Windows line endings
        size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        std::string name = line.substr(0, equals);
        std::string value = line.substr(equals + 1);

        try {
            if (name == "playerLevel") {
                profile.playerLevel = clampInt(std::stoi(value), 1, MAX_PLAYER_LEVEL);
            } else if (name == "playerXp") {
                profile.playerXp = clampInt(std::stoi(value), 0, xpNeededForPlayerLevel(profile.playerLevel) - 1);
            } else if (name.rfind("weapon.", 0) == 0) {
                int index = std::stoi(name.substr(7));
                std::istringstream in(value);
                WeaponProgress p;
                if (index >= 0 && index < (int)profile.weaponProgress.size() && (in >> p.level >> p.xp)) {
                    p.level = clampInt(p.level, 1, MAX_WEAPON_LEVEL);
                    p.xp = clampInt(p.xp, 0, xpNeededForLevel(p.level) - 1);
                    profile.weaponProgress[index] = p;
                }
            } else if (name == "loadouts") {
                loadouts.resize(clampInt(std::stoi(value), 0, 20));
            } else if (name.rfind("loadout.", 0) == 0) {
                // loadout.<number>.<part>
                size_t dot = name.find('.', 8);
                if (dot == std::string::npos) continue;
                int index = std::stoi(name.substr(8, dot - 8));
                std::string part = name.substr(dot + 1);
                if (index < 0 || index >= (int)loadouts.size()) continue;
                Loadout& l = loadouts[index];
                if (part == "name") {
                    l.name = value;
                } else if (part == "primary") {
                    readBuild(value, l.primary, true);
                } else if (part == "secondary") {
                    readBuild(value, l.secondary, false);
                } else if (part == "equipment") {
                    std::istringstream in(value);
                    int tactical = 0, lethal = 0;
                    in >> tactical >> lethal;
                    l.tactical = clampInt(tactical, 0, TACTICAL_COUNT - 1);
                    l.lethal = clampInt(lethal, 0, LETHAL_COUNT - 1);
                }
            }
        } catch (...) {
            TraceLog(LOG_WARNING, "Ignoring bad line in profile.cfg: %s", line.c_str());
        }
    }

    // Use the saved loadouts if they look complete; otherwise keep the starting ones
    bool good = !loadouts.empty();
    for (const Loadout& l : loadouts) {
        if (l.name.empty() || !WEAPONS[l.primary.weapon].isPrimary || WEAPONS[l.secondary.weapon].isPrimary) good = false;
    }
    if (good) profile.loadouts = loadouts;
    return profile;
}
