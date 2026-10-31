#include "Assets.h"
#include "rlgl.h"
#include <cmath>

const int CAMO_SIZE = 128;   // camo patterns are designed on a 128x128 grid

// ------------------------------------------------------------
// Noise: smooth random patterns, the secret ingredient of camo
// ------------------------------------------------------------

// A random-looking number from 0 to 1 that's always the same for the same x, y, seed
static float hash(int x, int y, int seed) {
    unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u + (unsigned int)seed * 982451653u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return (h & 0xFFFFFF) / (float)0xFFFFFF;
}

// Blends between random values on a grid, so it changes smoothly
static float valueNoise(float x, float y, int seed) {
    int ix = (int)floorf(x);
    int iy = (int)floorf(y);
    float fx = x - ix;
    float fy = y - iy;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    float a = hash(ix, iy, seed);
    float b = hash(ix + 1, iy, seed);
    float c = hash(ix, iy + 1, seed);
    float d = hash(ix + 1, iy + 1, seed);
    float top = a + (b - a) * fx;
    float bottom = c + (d - c) * fx;
    return top + (bottom - top) * fy;
}

// Several layers of noise added together: big blobs with small wobbly edges
static float layeredNoise(float x, float y, int seed) {
    return valueNoise(x, y, seed) * 0.6f
         + valueNoise(x * 2, y * 2, seed + 1) * 0.3f
         + valueNoise(x * 4, y * 4, seed + 2) * 0.1f;
}

static Color pick3(float n, const Camo& camo) {
    if (n < 0.42f) return camo.colorB;
    if (n > 0.62f) return camo.colorC;
    return camo.colorA;
}

static Color shade(Color c, float amount) {
    auto apply = [&](unsigned char v) {
        float result = v * amount;
        return (unsigned char)(result > 255 ? 255 : result);
    };
    return { apply(c.r), apply(c.g), apply(c.b), 255 };
}

static Color camoPixel(const Camo& camo, int x, int y, int seed) {
    switch (camo.pattern) {
        case PATTERN_BLOTCH:
            return pick3(layeredNoise(x / 28.0f, y / 28.0f, seed), camo);

        case PATTERN_DIGITAL: {
            int cellX = x / 8, cellY = y / 8;   // chunky square pixels
            return pick3(layeredNoise(cellX / 3.5f, cellY / 3.5f, seed), camo);
        }

        case PATTERN_STRIPES: {
            float wobble = layeredNoise(x / 20.0f, y / 20.0f, seed) * 6;
            float wave = sinf((x + y * 0.35f) / 7.0f + wobble);
            return wave > 0.45f ? camo.colorB : camo.colorA;
        }

        case PATTERN_SPLINTER: {
            float u = x * 0.87f + y * 0.5f;    // tilt the grid 30 degrees
            float v = -x * 0.5f + y * 0.87f;
            float n = hash((int)floorf(u / 22), (int)floorf(v / 10), seed);
            return pick3(n, camo);
        }

        case PATTERN_WEAVE: {
            bool across = ((x / 8) + (y / 8)) % 2 == 0;
            float t = across ? (x % 8) / 8.0f : (y % 8) / 8.0f;
            Color base = across ? camo.colorA : camo.colorB;
            return shade(base, 0.75f + 0.5f * sinf(t * 3.14159f));
        }

        case PATTERN_SCULPTEDGILD: {
			bool across = ((x / 12) + (y / 16)) % 4 < 2;
			float t = across ? (x % 12) / 12.0f : (y % 16) / 16.0f;
			Color base = across ? camo.colorA : camo.colorB;
			return shade(base, 0.75f + 0.5f * sinf(t * 3.14159f));
        }

        case PATTERN_VEINS: {
            // Find the two closest random points: where they're nearly equal, draw a glowing crack
            float closest = 9999, second = 9999;
            int cellX = x / 32, cellY = y / 32;
            for (int oy = -1; oy <= 1; oy++) {
                for (int ox = -1; ox <= 1; ox++) {
                    int cx = cellX + ox, cy = cellY + oy;
                    float px = (cx + hash(cx, cy, seed)) * 32;
                    float py = (cy + hash(cx, cy, seed + 7)) * 32;
                    float d = sqrtf((px - x) * (px - x) + (py - y) * (py - y));
                    if (d < closest) { second = closest; closest = d; }
                    else if (d < second) { second = d; }
                }
            }
            float edge = second - closest;
            if (edge < 1.5f) return camo.colorC;
            if (edge < 4.0f) return camo.colorB;
            return shade(camo.colorA, 0.8f + 0.4f * valueNoise(x / 6.0f, y / 6.0f, seed));
        }

        default: {   // PATTERN_SOLID: a tiny bit of grain so it doesn't look flat
            float grain = 0.92f + 0.12f * valueNoise(x / 3.0f, y / 3.0f, seed);
            return shade(camo.colorA, grain);
        }
    }
}

// `size` is 128 for high quality or 64 for low. At 64 we skip every other
// pattern pixel, so the camo looks the same, just less detailed.
static Texture2D makeCamoTexture(const Camo& camo, int seed, int size) {
    int step = CAMO_SIZE / size;
    Color* pixels = (Color*)MemAlloc(size * size * sizeof(Color));
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            pixels[y * size + x] = camoPixel(camo, x * step, y * step, seed);
        }
    }
    Image image = { pixels, size, size, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);   // the GPU has its own copy now
    return texture;
}

void Assets::load(bool highTextures) {
    cube = LoadModelFromMesh(GenMeshCube(1, 1, 1));

    Image whiteImage = GenImageColor(4, 4, WHITE);
    white = LoadTextureFromImage(whiteImage);
    UnloadImage(whiteImage);

    reloadCamos(highTextures);
}

void Assets::unloadCamos() {
    for (Texture2D& t : camoTextures) {
        UnloadTexture(t);
    }
    camoTextures.clear();
}

void Assets::reloadCamos(bool highTextures) {
    unloadCamos();
    int size = highTextures ? CAMO_SIZE : CAMO_SIZE / 2;
    for (size_t i = 0; i < CAMOS.size(); i++) {
        camoTextures.push_back(makeCamoTexture(CAMOS[i], (int)i * 31 + 5, size));
    }
}

void Assets::unload() {
    unloadCamos();
    UnloadTexture(white);
    // The cube's material would try to unload our textures too, so point it back at the default first
    cube.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = { rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    UnloadModel(cube);
}

// Draw one box-shaped part, optionally tilted around the forward axis
static float partAngle = 0;

void Assets::part(Vector3 center, Vector3 size, Color tint, const Texture2D* texture) const {
    cube.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture ? *texture : white;
    DrawModelEx(cube, center, { 0, 0, 1 }, partAngle, size, tint);
    rlPushMatrix();
    rlTranslatef(center.x, center.y, center.z);
    rlRotatef(partAngle, 0, 0, 1);
    DrawCubeWiresV({ 0, 0, 0 }, size, Fade(BLACK, 0.35f));
    rlPopMatrix();
}

// Weapon parts are placed along the x axis: the muzzle points toward +x
void Assets::drawWeapon(const WeaponBuild& build, Vector3 position, float yaw, float scale) const {
    const WeaponDef& weapon = WEAPONS[build.weapon];
    const Texture2D* camo = &camoTextures[build.camo];
    const Color metal = { 52, 54, 58, 255 };
    const Color darkMetal = { 32, 33, 36, 255 };
    const Color white3 = WHITE;
    float L = weapon.bodyLength;

    auto look = [&](int slot) {
        int index = build.attachment[slot];
        return index < 0 ? 0 : weapon.attachments[index].look;
    };

    rlPushMatrix();
    rlTranslatef(position.x, position.y, position.z);
    rlRotatef(yaw, 0, 1, 0);
    rlScalef(scale, scale, scale);

    // Body
    part({ 0, 0, 0 }, { L * 0.55f, 0.16f, 0.07f }, white3, camo);                  // receiver
    part({ 0.02f, 0.095f, 0 }, { L * 0.52f, 0.03f, 0.05f }, darkMetal, nullptr);    // top rail
    float handguardEnd = L * 0.62f;
    part({ L * 0.42f, 0.01f, 0 }, { L * 0.4f, 0.12f, 0.075f }, white3, camo);       // handguard

    // Barrel
    int barrel = look(SLOT_BARREL);
    float barrelLength = (barrel == 1) ? L * 0.1f : (barrel == 2) ? L * 0.42f : (barrel == 3) ? L * 0.34f : L * 0.24f;
    float barrelThickness = (barrel == 3) ? 0.055f : 0.035f;
    part({ handguardEnd + barrelLength / 2, 0.02f, 0 }, { barrelLength, barrelThickness, barrelThickness }, darkMetal, nullptr);
    float muzzleX = handguardEnd + barrelLength;

    // Muzzle
    int muzzle = look(SLOT_MUZZLE);
    if (muzzle == 1) part({ muzzleX + 0.04f, 0.02f, 0 }, { 0.08f, 0.055f, 0.055f }, metal, nullptr);
    if (muzzle == 2) part({ muzzleX + 0.15f, 0.02f, 0 }, { 0.3f, 0.07f, 0.07f }, darkMetal, nullptr);
    if (muzzle == 3) part({ muzzleX + 0.03f, 0.02f, 0 }, { 0.06f, 0.045f, 0.045f }, metal, nullptr);
    if (muzzle == 4) part({ muzzleX + 0.06f, 0.02f, 0 }, { 0.12, 0.065, 0.065f }, metal, nullptr);

    // Pistol grip (tilted back a little)
    partAngle = 15;
    part({ -L * 0.1f, -0.13f, 0 }, { 0.06f, 0.16f, 0.06f }, darkMetal, nullptr);
    partAngle = 0;

    // Magazine
    int mag = look(SLOT_MAGAZINE);
    float magX = L * 0.08f;
    if (mag == 2) {
        part({ magX, -0.15f, 0 }, { 0.18f, 0.18f, 0.11f }, white3, camo);              // drum
    } else {
        float magLength = (mag == 1) ? 0.28f : 0.18f;
        partAngle = 10;
        part({ magX, -0.06f - magLength / 2, 0 }, { 0.07f, magLength, 0.05f }, white3, camo);
        partAngle = 0;
        if (mag == 3) part({ magX + 0.005f, -0.17f, 0 }, { 0.075f, 0.025f, 0.055f }, { 230, 180, 40, 255 }, nullptr);
    }

    // Stock
    int stock = look(SLOT_STOCK);
    if (stock == 0) part({ -L * 0.38f, -0.01f, 0 }, { L * 0.22f, 0.12f, 0.06f }, white3, camo);
    if (stock == 1) {
        part({ -L * 0.38f, 0.03f, 0 },  { L * 0.22f, 0.025f, 0.04f }, darkMetal, nullptr);
        part({ -L * 0.38f, -0.05f, 0 }, { L * 0.22f, 0.025f, 0.04f }, darkMetal, nullptr);
        part({ -L * 0.49f, -0.01f, 0 }, { 0.03f, 0.13f, 0.05f }, darkMetal, nullptr);
    }
    if (stock == 2) {
        part({ -L * 0.4f, -0.01f, 0 }, { L * 0.26f, 0.16f, 0.08f }, white3, camo);
        part({ -L * 0.38f, 0.085f, 0 }, { L * 0.14f, 0.03f, 0.06f }, darkMetal, nullptr);   // cheek rest
    }
    if (stock == 3) part({ -L * 0.31f, 0.01f, 0 }, { L * 0.08f, 0.04f, 0.04f }, darkMetal, nullptr);

    // Optic on the top rail
    int optic = look(SLOT_OPTIC);
    if (optic == 0) {   // iron sights
        part({ -L * 0.2f, 0.135f, 0 }, { 0.03f, 0.05f, 0.03f }, darkMetal, nullptr);
        part({ L * 0.58f, 0.115f, 0 }, { 0.02f, 0.07f, 0.02f }, darkMetal, nullptr);
    }
    if (optic == 1) {
        part({ 0, 0.14f, 0 }, { 0.08f, 0.06f, 0.05f }, metal, nullptr);
        part({ 0.041f, 0.15f, 0 }, { 0.005f, 0.02f, 0.02f }, RED, nullptr);
    }
    if (optic == 2) {
        part({ 0, 0.125f, 0 }, { 0.12f, 0.03f, 0.06f }, metal, nullptr);
        part({ 0.04f, 0.17f, 0 }, { 0.02f, 0.07f, 0.065f }, metal, nullptr);
    }
    if (optic == 3) {
        part({ 0, 0.16f, 0 }, { 0.36f, 0.065f, 0.065f }, darkMetal, nullptr);
        part({ 0.16f, 0.16f, 0 }, { 0.06f, 0.085f, 0.085f }, darkMetal, nullptr);
        part({ -0.15f, 0.16f, 0 }, { 0.05f, 0.08f, 0.08f }, darkMetal, nullptr);
    }

    // Laser on the side of the handguard
    int laser = look(SLOT_LASER);
    if (laser == 1) {
        part({ L * 0.45f, 0.01f, 0.055f }, { 0.08f, 0.035f, 0.03f }, metal, nullptr);
        DrawLine3D({ L * 0.49f, 0.01f, 0.055f }, { L * 0.49f + 1.5f, 0.01f, 0.055f }, RED);
    }
    if (laser == 2) {
        part({ L * 0.45f, 0.01f, 0.055f }, { 0.1f, 0.04f, 0.04f }, metal, nullptr);
        part({ L * 0.5f + 0.001f, 0.01f, 0.055f }, { 0.004f, 0.034f, 0.034f }, { 255, 250, 210, 255 }, nullptr);
    }

    // Grip under the handguard
    int underbarrel = look(SLOT_UNDERBARREL);
    if (underbarrel == 1) part({ L * 0.42f, -0.11f, 0 }, { 0.045f, 0.13f, 0.045f }, darkMetal, nullptr);
    if (underbarrel == 2) {
        partAngle = -35;
        part({ L * 0.38f, -0.07f, 0 }, { 0.11f, 0.045f, 0.045f }, darkMetal, nullptr);
        partAngle = 0;
    }

    rlPopMatrix();
}

Vector3 Assets::slotAnchor(const WeaponBuild& build, int slot, Vector3 position, float yaw, float scale) const {
    float L = WEAPONS[build.weapon].bodyLength;
    Vector3 local = { 0, 0, 0 };
    switch (slot) {
        case SLOT_MUZZLE:      local = { L * 0.95f, 0.02f, 0 }; break;
        case SLOT_BARREL:      local = { L * 0.72f, 0.02f, 0 }; break;
        case SLOT_LASER:       local = { L * 0.45f, 0.01f, 0.06f }; break;
        case SLOT_OPTIC:       local = { 0, 0.17f, 0 }; break;
        case SLOT_UNDERBARREL: local = { L * 0.42f, -0.12f, 0 }; break;
        case SLOT_MAGAZINE:    local = { L * 0.08f, -0.2f, 0 }; break;
        case SLOT_STOCK:       local = { -L * 0.4f, 0, 0 }; break;
    }
    // Turn the point the same way the weapon is turned (rotation around the y axis)
    float radians = yaw * DEG2RAD;
    float x = local.x * cosf(radians) + local.z * sinf(radians);
    float z = -local.x * sinf(radians) + local.z * cosf(radians);
    return { position.x + x * scale, position.y + local.y * scale, position.z + z * scale };
}

void Assets::drawSoldier(const WeaponBuild& weapon, Vector3 position, float yaw, float time,
                         const SoldierStyle& style) const {
    const Color boots = { 30, 28, 26, 255 };
    const Color gloves = { 40, 38, 34, 255 };
    const Color helmet = { 70, 74, 62, 255 };
    const Color face = { 45, 45, 48, 255 };       // balaclava

    float breathe = sinf(time * 2.0f) * 0.008f;

    rlPushMatrix();
    rlTranslatef(position.x, position.y, position.z);
    rlRotatef(yaw, 0, 1, 0);

    // Legs: each one swings from the hip while running
    for (int side = -1; side <= 1; side += 2) {
        float x = 0.12f * side;
        rlPushMatrix();
        rlTranslatef(x, 0.9f, 0);
        rlRotatef(style.stride * 32 * side, 1, 0, 0);
        part({ 0, -0.4f, 0 }, { 0.16f, 0.78f, 0.18f }, style.uniform, nullptr);
        part({ 0, -0.84f, 0.02f }, { 0.14f, 0.12f, 0.24f }, boots, nullptr);
        rlPopMatrix();
    }
    part({ 0, 0.92f, 0 }, { 0.42f, 0.14f, 0.22f }, style.uniform, nullptr);    // hips

    // Torso and vest
    part({ 0, 1.2f + breathe, 0 }, { 0.46f, 0.46f, 0.24f }, style.uniform, nullptr);
    part({ 0, 1.2f + breathe, 0.02f }, { 0.48f, 0.4f, 0.26f }, style.vest, nullptr);
    part({ -0.12f, 1.12f + breathe, 0.15f }, { 0.1f, 0.12f, 0.05f }, style.vest, nullptr);   // pouches
    part({ 0.0f, 1.12f + breathe, 0.15f },   { 0.1f, 0.12f, 0.05f }, style.vest, nullptr);
    part({ 0.12f, 1.12f + breathe, 0.15f },  { 0.1f, 0.12f, 0.05f }, style.vest, nullptr);
    part({ -0.27f, 1.32f + breathe, 0 }, { 0.04f, 0.08f, 0.1f }, style.patch, nullptr);       // shoulder patch

    // Head
    part({ 0, 1.58f + breathe, 0 }, { 0.2f, 0.24f, 0.22f }, face, nullptr);
    part({ 0, 1.71f + breathe, -0.01f }, { 0.26f, 0.1f, 0.27f }, helmet, nullptr);
    part({ 0, 1.6f + breathe, 0.115f }, { 0.18f, 0.05f, 0.02f }, style.visor, nullptr);

    if (style.aimingForward) {
        // Arms raised, weapon pointing straight ahead (the soldier faces +z)
        part({ -0.25f, 1.27f + breathe, 0.1f }, { 0.12f, 0.13f, 0.3f }, style.uniform, nullptr);   // right arm
        part({ -0.16f, 1.23f + breathe, 0.3f }, { 0.1f, 0.1f, 0.2f }, style.uniform, nullptr);
        part({ -0.1f, 1.22f + breathe, 0.4f }, { 0.08f, 0.08f, 0.08f }, gloves, nullptr);
        part({ 0.25f, 1.27f + breathe, 0.14f }, { 0.12f, 0.13f, 0.34f }, style.uniform, nullptr);   // left arm
        part({ 0.12f, 1.27f + breathe, 0.42f }, { 0.1f, 0.1f, 0.3f }, style.uniform, nullptr);
        part({ -0.02f, 1.28f + breathe, 0.6f }, { 0.08f, 0.08f, 0.08f }, gloves, nullptr);
        part({ 0.27f, 1.3f + breathe, 0.06f }, { 0.13f, 0.06f, 0.15f }, style.patch, nullptr);     // armband
        drawWeapon(weapon, { -0.07f, 1.33f + breathe, 0.45f }, -90, 0.85f);
    } else {
        // Arms reaching for the weapon
        part({ -0.27f, 1.2f + breathe, 0.06f }, { 0.12f, 0.36f, 0.14f }, style.uniform, nullptr);
        part({ 0.27f, 1.2f + breathe, 0.06f },  { 0.12f, 0.36f, 0.14f }, style.uniform, nullptr);
        part({ -0.18f, 1.02f + breathe, 0.2f }, { 0.1f, 0.1f, 0.26f }, style.uniform, nullptr);
        part({ 0.2f, 1.06f + breathe, 0.2f },   { 0.1f, 0.1f, 0.26f }, style.uniform, nullptr);
        part({ -0.13f, 1.0f + breathe, 0.33f }, { 0.08f, 0.08f, 0.08f }, gloves, nullptr);
        part({ 0.2f, 1.04f + breathe, 0.33f },  { 0.08f, 0.08f, 0.08f }, gloves, nullptr);

        // The weapon, held across the chest pointing down-left
        drawWeapon(weapon, { 0.02f, 1.05f + breathe, 0.34f }, -150, 0.85f);
    }

    rlPopMatrix();
}

void Assets::drawCamoSwatch(int camo, Rectangle r) const {
    const Texture2D& t = camoTextures[camo];
    DrawTexturePro(t, { 0, 0, (float)t.width, (float)t.height }, r, { 0, 0 }, 0, WHITE);
}
