#include "GameData.h"

std::vector<WeaponDef> WEAPONS;
std::vector<Camo> CAMOS;
const char* TACTICALS[] = { "Flash Grenade", "Smoke Grenade", "Stun Grenade", "Decoy" };
const char* LETHALS[] = { "Frag Grenade", "Sticky Grenade", "Throwing Knife", "Claymore" };

const char* slotName(int slot) {
    switch (slot) {
        case SLOT_MUZZLE:      return "MUZZLE";
        case SLOT_BARREL:      return "BARREL";
        case SLOT_LASER:       return "LASER";
        case SLOT_OPTIC:       return "OPTIC";
        case SLOT_UNDERBARREL: return "UNDERBARREL";
        case SLOT_MAGAZINE:    return "MAGAZINE";
        case SLOT_STOCK:       return "STOCK";
    }
    return "?";
}

WeaponBuild::WeaponBuild() {
    for (int i = 0; i < SLOT_COUNT; i++) {
        attachment[i] = -1;
    }
}

int WeaponBuild::attachmentCount() const {
    int count = 0;
    for (int i = 0; i < SLOT_COUNT; i++) {
        if (attachment[i] >= 0) count++;
    }
    return count;
}

// Helper to keep the attachment lists short and readable
static void add(WeaponDef& weapon, const char* name, const char* type, int slot, int level,
                Stats change, int look, int magazineSize = 0) {
    Attachment a;
    a.name = name;
    a.type = type;
    a.slot = slot;
    a.unlockLevel = level;
    a.change = change;
    a.look = look;
    a.magazineSize = magazineSize;
    weapon.attachments.push_back(a);
}

// Stats order: { damage, range, accuracy, mobility, handling }
static void addStandardAttachments(WeaponDef& w, int bigMag, int drumMag) {
    add(w, "Brake-7 Compensator", "Compensator", SLOT_MUZZLE, 3, {0, 0, 8, 0, -2}, 1);
    add(w, "Hush-40 Suppressor",  "Suppressor",    SLOT_MUZZLE, 9,  { 0, 5, 3, -3, -4 }, 2);
    add(w, "Starburst Hider",     "Flash Hider",   SLOT_MUZZLE, 17, { 0, 0, 4, 0, 2 }, 3);

    add(w, "Raptor 11\" Barrel",  "Short Barrel",  SLOT_BARREL, 5,  { 0, -6, -2, 6, 6 }, 1);
    add(w, "Longreach 20\" Barrel", "Long Barrel", SLOT_BARREL, 12, { 2, 12, 4, -5, -6 }, 2);
    add(w, "Anvil Heavy Barrel",  "Heavy Barrel",  SLOT_BARREL, 21, { 4, 8, 6, -8, -8 }, 3);

    add(w, "Viper Red Laser",     "Laser",         SLOT_LASER, 4,  { 0, 0, 4, 0, 5 }, 1);
    add(w, "Glare Tac Light",     "Tactical Light", SLOT_LASER, 14, { 0, 0, 2, 2, 3 }, 2);

    add(w, "Pinpoint Red Dot",    "Red Dot",       SLOT_OPTIC, 2,  { 0, 0, 5, 0, 0 }, 1);
    add(w, "Halo Holographic",    "Holo Sight",    SLOT_OPTIC, 10, { 0, 2, 7, 0, -1 }, 2);
    add(w, "Hawkeye 4x Scope",    "Scope",         SLOT_OPTIC, 19, { 0, 6, 9, -2, -5 }, 3);

    add(w, "Steady Vertical Grip", "Vertical Grip", SLOT_UNDERBARREL, 6,  { 0, 0, 8, -1, -1 }, 1);
    add(w, "Swift Angled Grip",   "Angled Grip",   SLOT_UNDERBARREL, 15, { 0, 0, 4, 0, 5 }, 2);

    add(w, "Extended Mag",        "Magazine",      SLOT_MAGAZINE, 7,  { 0, 0, 0, -2, -3 }, 1, bigMag);
    add(w, "Hellfire Drum",       "Drum Magazine", SLOT_MAGAZINE, 24, { 0, 0, 0, -6, -8 }, 2, drumMag);
    add(w, "Quickpull Mag",       "Fast Magazine", SLOT_MAGAZINE, 13, { 0, 0, 0, 1, 6 }, 3);

    add(w, "Featherweight Stock", "Light Stock",   SLOT_STOCK, 8,  { 0, 0, -3, 6, 4 }, 1);
    add(w, "Bulwark Stock",       "Heavy Stock",   SLOT_STOCK, 16, { 0, 0, 7, -4, -3 }, 2);
    add(w, "No Stock",            "Stock Removal", SLOT_STOCK, 26, { 0, 0, -8, 10, 8 }, 3);
}

static void addCamo(const char* name, const char* category, const char* challenge, int level,
                    int pattern, Color a, Color b, Color c) {
    CAMOS.push_back({ name, category, challenge, level, pattern, a, b, c });
}

void setupGameData() {
    WEAPONS.clear();
    CAMOS.clear();

    // Stats order: { damage, range, accuracy, mobility, handling }
    WeaponDef rifle;
    rifle.name = "VK-4 RANGER";
    rifle.weaponClass = "Assault Rifle";
    rifle.base = { 28, 45, 48, 55, 58 };
    rifle.fireRate = 780;
    rifle.magazineSize = 30;
    rifle.isPrimary = true;
    rifle.bodyLength = 1.0f;
    addStandardAttachments(rifle, 45, 60);
    WEAPONS.push_back(rifle);

    WeaponDef smg;
    smg.name = "WASP-9";
    smg.weaponClass = "SMG";
    smg.base = { 22, 28, 42, 72, 70 };
    smg.fireRate = 950;
    smg.magazineSize = 32;
    smg.isPrimary = true;
    smg.bodyLength = 0.75f;
    addStandardAttachments(smg, 45, 64);
    WEAPONS.push_back(smg);

    WeaponDef sniper;
    sniper.name = "LONGBOW .338";
    sniper.weaponClass = "Sniper Rifle";
    sniper.base = { 90, 85, 62, 35, 30 };
    sniper.fireRate = 45;
    sniper.magazineSize = 5;
    sniper.isPrimary = false;
    sniper.bodyLength = 1.65f;
    addStandardAttachments(sniper, 8, 12);
    WEAPONS.push_back(sniper);

    WeaponDef pistol;
    pistol.name = "MAKO .45";
    pistol.weaponClass = "Pistol";
    pistol.base = { 32, 22, 40, 85, 80 };
    pistol.fireRate = 400;
    pistol.magazineSize = 10;
    pistol.isPrimary = false;
    pistol.bodyLength = 0.45f;
    addStandardAttachments(pistol, 15, 30);
    WEAPONS.push_back(pistol);

    WeaponDef shotgun;
    shotgun.name = "SHAWTY";
    shotgun.weaponClass = "Shotgun";
    shotgun.base = { 80, 20, 30, 40, 50 };
    shotgun.fireRate = 75;
    shotgun.magazineSize = 7;
    shotgun.isPrimary = true;
    shotgun.bodyLength = 1.4f;
    addStandardAttachments(shotgun, 10, 20);
    add(shotgun, "Thunder & Lightning Comp", "Compensator", SLOT_MUZZLE, 14, { 0, -3, 6, 3, -1 }, 4);
    WEAPONS.push_back(shotgun);

    // ---- Camos ----
    addCamo("No Camo",      "MILITARY", "Default", 1, PATTERN_SOLID,
            { 45, 47, 45, 255 }, { 45, 47, 45, 255 }, { 45, 47, 45, 255 });
    addCamo("Pine Forest",  "MILITARY", "Reach weapon level 2", 2, PATTERN_BLOTCH,
            { 70, 85, 50, 255 }, { 40, 50, 30, 255 }, { 110, 95, 60, 255 });
    addCamo("Dune",         "MILITARY", "Reach weapon level 4", 4, PATTERN_BLOTCH,
            { 190, 165, 115, 255 }, { 150, 120, 80, 255 }, { 220, 200, 150, 255 });
    addCamo("City Pixel",   "MILITARY", "Reach weapon level 6", 6, PATTERN_DIGITAL,
            { 110, 112, 115, 255 }, { 70, 72, 76, 255 }, { 160, 160, 160, 255 });
    addCamo("Snowfield",    "MILITARY", "Reach weapon level 8", 8, PATTERN_DIGITAL,
            { 225, 230, 235, 255 }, { 170, 180, 190, 255 }, { 120, 130, 140, 255 });
    addCamo("Ember Tiger",  "MILITARY", "Reach weapon level 11", 11, PATTERN_STRIPES,
            { 200, 110, 40, 255 }, { 25, 20, 20, 255 }, { 200, 110, 40, 255 });
    addCamo("Swamp Tiger",  "MILITARY", "Reach weapon level 14", 14, PATTERN_STRIPES,
            { 80, 100, 50, 255 }, { 25, 35, 20, 255 }, { 120, 110, 60, 255 });
    addCamo("Shard",        "MILITARY", "Reach weapon level 17", 17, PATTERN_SPLINTER,
            { 120, 125, 100, 255 }, { 60, 70, 55, 255 }, { 170, 160, 130, 255 });

    addCamo("Carbon Weave", "SPECIAL", "Reach weapon level 20", 20, PATTERN_WEAVE,
            { 40, 42, 46, 255 }, { 20, 20, 22, 255 }, { 70, 72, 78, 255 });
    addCamo("Night Ops",    "SPECIAL", "Reach weapon level 23", 23, PATTERN_DIGITAL,
            { 30, 40, 70, 255 }, { 15, 18, 35, 255 }, { 60, 80, 120, 255 });
    addCamo("Toxic",        "SPECIAL", "Reach weapon level 26", 26, PATTERN_BLOTCH,
            { 120, 200, 40, 255 }, { 30, 60, 20, 255 }, { 200, 240, 80, 255 });

    addCamo("Gilded",       "MASTERY", "Reach max weapon level (30)", 30, PATTERN_SOLID,
            { 212, 175, 55, 255 }, { 212, 175, 55, 255 }, { 212, 175, 55, 255 });
    addCamo("Gilded Sculpted", "MASTERY", "Reach max weapon level (30)", 30, PATTERN_SCULPTEDGILD,
            { 212, 175, 55, 255 }, { 212, 175, 55, 255 }, { 212, 175, 55, 255 });
    addCamo("Magma Vein",   "MASTERY", "Reach max weapon level (30)", 30, PATTERN_VEINS,
            { 20, 18, 18, 255 }, { 255, 90, 20, 255 }, { 255, 200, 60, 255 });
    addCamo("Acid Vein",    "MASTERY", "Reach max weapon level (30)", 30, PATTERN_VEINS,
            { 15, 20, 15, 255 }, { 60, 255, 60, 255 }, { 200, 255, 120, 255 });
}

Profile makeStartingProfile() {
    Profile profile;
    profile.playerLevel = 1;
    profile.weaponProgress.resize(WEAPONS.size());

    const char* names[] = { "ASSAULT", "RUN AND GUN", "MARKSMAN" };
    int primaries[] = { 0, 1, 0 };
    for (int i = 0; i < 3; i++) {
        Loadout loadout;
        loadout.name = names[i];
        loadout.primary.weapon = primaries[i];
        loadout.secondary.weapon = (i == 2) ? 2 : 3;
        loadout.tactical = i % TACTICAL_COUNT;
        loadout.lethal = i % LETHAL_COUNT;
        profile.loadouts.push_back(loadout);
    }
    return profile;
}

int xpNeededForLevel(int level) {
    return 800 + level * 200;
}

void addWeaponXp(WeaponProgress& progress, int amount) {
    progress.xp += amount;
    while (progress.level < MAX_WEAPON_LEVEL && progress.xp >= xpNeededForLevel(progress.level)) {
        progress.xp -= xpNeededForLevel(progress.level);
        progress.level++;
    }
    if (progress.level == MAX_WEAPON_LEVEL) {
        progress.xp = 0;
    }
}

static int clampStat(int value) {
    if (value < 0) return 0;
    if (value > 100) return 100;
    return value;
}

Stats statsWithAttachments(const WeaponBuild& build) {
    const WeaponDef& weapon = WEAPONS[build.weapon];
    Stats s = weapon.base;
    for (int slot = 0; slot < SLOT_COUNT; slot++) {
        int index = build.attachment[slot];
        if (index < 0) continue;
        const Stats& c = weapon.attachments[index].change;
        s.damage += c.damage;
        s.range += c.range;
        s.accuracy += c.accuracy;
        s.mobility += c.mobility;
        s.handling += c.handling;
    }
    s.damage = clampStat(s.damage);
    s.range = clampStat(s.range);
    s.accuracy = clampStat(s.accuracy);
    s.mobility = clampStat(s.mobility);
    s.handling = clampStat(s.handling);
    return s;
}

int magazineWithAttachments(const WeaponBuild& build) {
    const WeaponDef& weapon = WEAPONS[build.weapon];
    int index = build.attachment[SLOT_MAGAZINE];
    if (index >= 0 && weapon.attachments[index].magazineSize > 0) {
        return weapon.attachments[index].magazineSize;
    }
    return weapon.magazineSize;
}
