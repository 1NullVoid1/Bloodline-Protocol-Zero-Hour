#pragma once

#include "raylib.h"
#include "GameData.h"
#include <vector>

// The 3D placeholder art: a textured cube we stretch into weapon parts
// and soldier body parts, plus one generated texture per camo.
// Later you can swap these for real 3D models made in Blender.
struct Assets {
    Model cube;                          // a 1x1x1 cube we scale into shapes
    Texture2D white;                     // plain texture for untextured parts
    std::vector<Texture2D> camoTextures; // one per entry in CAMOS

    void load();
    void unload();

    // Draw a weapon with its attachments, centered at `position`, turned by `yaw` degrees
    void drawWeapon(const WeaponBuild& build, Vector3 position, float yaw, float scale) const;
    // Draw the player's soldier holding their weapon
    void drawSoldier(const WeaponBuild& weapon, Vector3 position, float yaw, float time) const;

    // Where a slot's attachment sits on the weapon (used to point labels at it)
    Vector3 slotAnchor(const WeaponBuild& build, int slot, Vector3 position, float yaw, float scale) const;

    // Draw a camo swatch in 2D (for the camo grid)
    void drawCamoSwatch(int camo, Rectangle r) const;

private:
    void part(Vector3 center, Vector3 size, Color tint, const Texture2D* texture) const;
};
