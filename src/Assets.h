#pragma once

#include "raylib.h"
#include "GameData.h"
#include <vector>

// How a soldier looks. The menus use the defaults; matches use team colors.
struct SoldierStyle {
    Color uniform = { 58, 62, 52, 255 };
    Color vest = { 85, 90, 65, 255 };
    Color visor = { 60, 150, 140, 255 };
    Color patch = { 222, 178, 82, 255 };   // shoulder patch and armband
    bool aimingForward = false;            // hold the weapon pointing forward (in matches)
    float stride = 0;                      // leg swing while running, from -1 to 1
};

// The 3D placeholder art: a textured cube we stretch into weapon parts
// and soldier body parts, plus one generated texture per camo.
// Later you can swap these for real 3D models made in Blender.
struct Assets {
    Model cube;                          // a 1x1x1 cube we scale into shapes
    Texture2D white;                     // plain texture for untextured parts
    std::vector<Texture2D> camoTextures; // one per entry in CAMOS

    void load(bool highTextures);
    void unload();
    // Remake the camo textures at high (128x128) or low (64x64) quality
    void reloadCamos(bool highTextures);

    // Draw a weapon with its attachments, centered at `position`, turned by `yaw` degrees
    void drawWeapon(const WeaponBuild& build, Vector3 position, float yaw, float scale) const;
    // Draw the player's soldier holding their weapon
    void drawSoldier(const WeaponBuild& weapon, Vector3 position, float yaw, float time,
                     const SoldierStyle& style = SoldierStyle()) const;

    // Where a slot's attachment sits on the weapon (used to point labels at it)
    Vector3 slotAnchor(const WeaponBuild& build, int slot, Vector3 position, float yaw, float scale) const;

    // Draw a camo swatch in 2D (for the camo grid)
    void drawCamoSwatch(int camo, Rectangle r) const;

private:
    void unloadCamos();
    void part(Vector3 center, Vector3 size, Color tint, const Texture2D* texture) const;
};
