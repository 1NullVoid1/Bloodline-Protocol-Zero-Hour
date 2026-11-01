#pragma once

#include "raylib.h"
#include <string>
#include <vector>

// ------------------------------------------------------------
// Everything the menus show: weapons, attachments, camos and
// the player's saved loadouts. All names here are placeholders,
// rename them to whatever you like.
// ------------------------------------------------------------

const char* const GAME_TITLE = "Bloodline Protocol: Zero Hour";   // change this to your game's name!

// The places an attachment can go on a weapon
enum Slot { SLOT_MUZZLE, SLOT_BARREL, SLOT_LASER, SLOT_OPTIC,
            SLOT_UNDERBARREL, SLOT_MAGAZINE, SLOT_STOCK, SLOT_COUNT };

const char* slotName(int slot);

const int MAX_ATTACHMENTS = 5;   // how many slots can be filled at once
const int MAX_WEAPON_LEVEL = 30;
const int MAX_PLAYER_LEVEL = 55;


struct Stats {
    int damage = 0;
    int range = 0;
    int accuracy = 0;
    int mobility = 0;
    int handling = 0;
};

struct Attachment {
    std::string name;
    std::string type;       // short description, like "Long Barrel"
    int slot;
    int unlockLevel;        // weapon level needed to use it
    Stats change;           // how it changes the weapon's stats
    int magazineSize = 0;   // 0 = doesn't change the magazine
    int look = 0;           // which 3D shape to draw for it
};

// Camo patterns we know how to draw (see Assets.cpp)
enum CamoPattern { PATTERN_SOLID, PATTERN_BLOTCH, PATTERN_DIGITAL, PATTERN_STRIPES,
                   PATTERN_SPLINTER, PATTERN_WEAVE, PATTERN_VEINS, PATTERN_SCULPTEDGILD };

struct Camo {
    std::string name;
    std::string category;   // the group it's shown in
    std::string challenge;  // what you do to unlock it
    int unlockLevel;
    int pattern;
    Color colorA, colorB, colorC;
};

struct WeaponDef {
    std::string name;
    std::string weaponClass;   // "Assault Rifle", "SMG"...
    Stats base;
    int fireRate;              // rounds per minute
    int magazineSize;
    bool isPrimary;            // primaries go in slot 1, the rest in slot 2
    float bodyLength;          // for the 3D preview
    int unlockLevel;
    std::vector<Attachment> attachments;
};

// One weapon with your attachments and camo on it
struct WeaponBuild {
    int weapon = 0;
    int attachment[SLOT_COUNT];   // index into that weapon's attachments, -1 = empty
    int camo = 0;

    WeaponBuild();
    int attachmentCount() const;
};

struct Loadout {
    std::string name;
    WeaponBuild primary;
    WeaponBuild secondary;
    int tactical = 0;
    int lethal = 0;
};

struct WeaponProgress {
    int level = 1;
    int xp = 0;   // xp toward the next level
};

// Everything about the player that should be saved
struct Profile {
    int playerLevel = 1;
    int playerXp = 0;   // xp toward the next player level
    std::vector<Loadout> loadouts;
    std::vector<WeaponProgress> weaponProgress;   // one per weapon
};

// The game's data lists
extern std::vector<WeaponDef> WEAPONS;
extern std::vector<Camo> CAMOS;
extern const char* TACTICALS[];
extern const char* LETHALS[];
const int TACTICAL_COUNT = 4;
const int LETHAL_COUNT = 4;

void setupGameData();
Profile makeStartingProfile();

int xpNeededForLevel(int level);
void addWeaponXp(WeaponProgress& progress, int amount);
int xpNeededForPlayerLevel(int level);
void addPlayerXp(Profile& profile, int amount);
bool weaponUnlocked(const Profile& profile, int weapon);




// Saving your progress (SaveFile.cpp): profile.cfg next to game.exe
void saveProfile(const Profile& profile);
Profile loadProfile();   // reads profile.cfg, or makes a fresh profile if there isn't one
Stats statsWithAttachments(const WeaponBuild& build);
int attachmentLook(const WeaponBuild& build, int slot);   // the 3D shape in that slot (0 = nothing fitted)
int magazineWithAttachments(const WeaponBuild& build);
