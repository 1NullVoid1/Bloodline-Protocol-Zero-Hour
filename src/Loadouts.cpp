#include "App.h"
#include "Ui.h"
#include <cmath>

const int MAX_LOADOUTS = 8;

// Switch to the next/previous weapon that fits this slot (primary or secondary)
static void cycleWeapon(WeaponBuild& build, bool primarySlot, int direction) {
    int count = (int)WEAPONS.size();
    int next = build.weapon;
    for (int tries = 0; tries < count; tries++) {
        next = (next + direction + count) % count;
        if (WEAPONS[next].isPrimary == primarySlot) break;
    }
    build = WeaponBuild();   // new weapon: attachments and camo start empty
    build.weapon = next;
}

// A card showing one weapon. Returns true when the card itself is clicked.
static bool weaponCard(App& app, Rectangle r, const char* label, WeaponBuild& build, bool primarySlot) {
    const WeaponDef& weapon = WEAPONS[build.weapon];
    const WeaponProgress& progress = app.profile.weaponProgress[build.weapon];

    Rectangle leftArrow = { r.x + r.width - 74, r.y + 8, 30, 30 };
    Rectangle rightArrow = { r.x + r.width - 38, r.y + 8, 30, 30 };
    bool onArrows = ui::isHovered(leftArrow) || ui::isHovered(rightArrow);
    bool hovered = ui::isHovered(r) && !onArrows;

    ui::panel(r, hovered ? ui::PANEL_HOVER : ui::PANEL);
    if (hovered) ui::outline(r, 1, ui::ACCENT);

    ui::text(label, r.x + 16, r.y + 12, 16, ui::TEXT_DIM);
    ui::text(weapon.weaponClass.c_str(), r.x + 16, r.y + 56, 16, ui::OLIVE);
    ui::text(weapon.name.c_str(), r.x + 16, r.y + 76, 26, ui::TEXT);
    ui::textRight(TextFormat("LEVEL %d", progress.level), r.x + r.width - 16, r.y + 84, 16, ui::TEXT_DIM);

    // Attachment pips
    for (int i = 0; i < MAX_ATTACHMENTS; i++) {
        Color c = i < build.attachmentCount() ? ui::ACCENT : Fade(ui::TEXT_DIM, 0.4f);
        DrawRectangle((int)(r.x + 16 + i * 16), (int)(r.y + 38), 10, 10, c);
    }

    // Camo swatch
    app.assets.drawCamoSwatch(build.camo, { r.x + r.width - 110, r.y + 44, 26, 26 });

    if (ui::button(leftArrow, "<")) cycleWeapon(build, primarySlot, -1);
    if (ui::button(rightArrow, ">")) cycleWeapon(build, primarySlot, +1);

    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

// A small card for equipment; clicking it cycles through the choices
static void equipmentCard(Rectangle r, const char* label, const char* const* names, int count, int& choice) {
    bool hovered = ui::isHovered(r);
    ui::panel(r, hovered ? ui::PANEL_HOVER : ui::PANEL);
    ui::text(label, r.x + 16, r.y + 10, 14, ui::TEXT_DIM);
    ui::text(names[choice], r.x + 16, r.y + 30, 20, ui::TEXT);
    if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        choice = (choice + 1) % count;
    }
}

// Draws one weapon, side-on and sized to fit, inside the box `area`.
// `which` picks the render texture: 0 = primary, 1 = secondary. They need one each,
// otherwise the second weapon would paint over the first before it reaches the screen.
static void drawWeaponPreview(App& app, const WeaponBuild& build, Rectangle area, const char* label, int which) {
    static RenderTexture2D targets[2] = {};
    RenderTexture2D& target = targets[which];
    // Size it in real pixels (so it's sharp in big windows), times the render scale setting
    float pixels = ui::pixelScale() * ui::renderScalePercent() / 100.0f;
    int texW = (int)(area.width * pixels);
    int texH = (int)(area.height * pixels);
    if (target.id == 0 || target.texture.width != texW || target.texture.height != texH) {
        if (target.id != 0) UnloadRenderTexture(target);
        target = LoadRenderTexture(texW, texH);
        SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    }

    // Same size for every weapon so long guns look long, but shrink one
    // if it wouldn't fit. Stock to muzzle is about 1.35 x bodyLength + 0.3.
    float length = WEAPONS[build.weapon].bodyLength;
    float scale = fminf(2.4f, 3.9f / (length * 1.35f + 0.3f));
    // The weapon's middle is a bit in front of its origin, so slide it back to center it
    Vector3 position = { -length * 0.19f * scale, -0.05f, 0 };

    Camera3D camera = {};
    camera.position = { 0, 0.25f, 3.0f };
    camera.target = { 0, 0, 0 };
    camera.up = { 0, 1, 0 };
    camera.fovy = 34;
    camera.projection = CAMERA_PERSPECTIVE;

    BeginTextureMode(target);
    ClearBackground(BLANK);
    BeginMode3D(camera);
    app.assets.drawWeapon(build, position, -12 + app.previewYaw, scale);
    EndMode3D();
    EndTextureMode();
    ui::restoreCanvas();   // EndTextureMode switched back to the unscaled window, so scale it again

    // A soft lit backdrop so the dark metal stands out
    ui::verticalGradient(area, Fade(ui::TEXT_DIM, 0.24f), Fade(ui::TEXT_DIM, 0.06f));
    ui::outline(area, 1, Fade(ui::TEXT_DIM, 0.25f));
    ui::text(label, area.x + 14, area.y + 10, 14, ui::TEXT_DIM);
    ui::text(WEAPONS[build.weapon].name.c_str(), area.x + 14, area.y + 26, 20, ui::TEXT);

    // Render textures are stored upside down, so flip the height when drawing
    Rectangle source = { 0, 0, (float)target.texture.width, -(float)target.texture.height };
    DrawTexturePro(target.texture, source, area, { 0, 0 }, 0, WHITE);
}

void updateLoadouts(App& app) {
    int w = ui::WIDTH;
    int h = ui::HEIGHT;

    ui::verticalGradient({ 0, 0, (float)w, (float)h }, { 24, 26, 24, 255 }, { 8, 8, 8, 255 });
    DrawCircleGradient(w - 300, h / 2 + 80, 380, Fade(ui::TEXT_DIM, 0.12f), Fade(ui::TEXT_DIM, 0.0f));

    // Weapon previews on the right: primary on top, secondary below, seen from the side.
    // Each one is drawn into its own box using a render texture (an off-screen picture).
    Rectangle previewArea = { 790, 100, 470, (float)h - 160 };
    handlePreviewDrag(app, previewArea);
    Loadout& loadout = app.profile.loadouts[app.selectedLoadout];
    drawWeaponPreview(app, loadout.primary, { previewArea.x, 110, previewArea.width, 210 }, "PRIMARY", 0);
    drawWeaponPreview(app, loadout.secondary, { previewArea.x, 330, previewArea.width, 210 }, "SECONDARY", 1);

    drawTopBar(app, app.mode == GameMode::Zombies ? "ZOMBIES" : "MULTIPLAYER", "LOADOUTS");
    if (ui::tab({ 470, 30, 150, 36 }, "PLAY", false)) {
        app.screen = Screen::Lobby;
    }
    ui::tab({ 626, 30, 150, 36 }, "LOADOUTS", true);

    // Left column: the list of loadouts
    float y = 110;
    for (int i = 0; i < (int)app.profile.loadouts.size(); i++) {
        const char* label = TextFormat("%02d  %s", i + 1, app.profile.loadouts[i].name.c_str());
        if (ui::menuButton({ 40, y, 270, 48 }, label, nullptr, i == app.selectedLoadout, true)) {
            app.selectedLoadout = i;
        }
        y += 54;
    }
    if ((int)app.profile.loadouts.size() < MAX_LOADOUTS) {
        if (ui::menuButton({ 40, y, 270, 48 }, "+  NEW LOADOUT", nullptr, false, true)) {
            Loadout fresh;
            fresh.name = TextFormat("CUSTOM %d", (int)app.profile.loadouts.size() + 1);
            fresh.primary.weapon = 0;
            fresh.secondary.weapon = 3;
            app.profile.loadouts.push_back(fresh);
            app.selectedLoadout = (int)app.profile.loadouts.size() - 1;
            // push_back can move the whole list to new memory, so the
            // 'loadout' reference above now points at freed memory.
            // Stop this frame here; next frame grabs a fresh reference.
            return;
        }
    }

    // Middle column: what's in the selected loadout
    float x = 340;
    float width = 420;
    if (weaponCard(app, { x, 110, width, 116 }, "PRIMARY", loadout.primary, true)) {
        app.editingPrimary = true;
        app.workshopTab = WorkshopTab::Attachments;
        app.selectedSlot = -1;
        app.selectedCamo = loadout.primary.camo;
        app.previewYaw = 0;
        app.screen = Screen::Workshop;
    }
    if (weaponCard(app, { x, 234, width, 116 }, "SECONDARY", loadout.secondary, false)) {
        app.editingPrimary = false;
        app.workshopTab = WorkshopTab::Attachments;
        app.selectedSlot = -1;
        app.selectedCamo = loadout.secondary.camo;
        app.previewYaw = 0;
        app.screen = Screen::Workshop;
    }

    Rectangle melee = { x, 358, width, 64 };
    ui::panel(melee, ui::PANEL);
    ui::text("MELEE", melee.x + 16, melee.y + 10, 14, ui::TEXT_DIM);
    ui::text("Combat Knife", melee.x + 16, melee.y + 30, 20, ui::TEXT);

    equipmentCard({ x, 430, width / 2 - 4, 64 }, "TACTICAL", TACTICALS, TACTICAL_COUNT, loadout.tactical);
    equipmentCard({ x + width / 2 + 4, 430, width / 2 - 4, 64 }, "LETHAL", LETHALS, LETHAL_COUNT, loadout.lethal);

    ui::text("Click a weapon to customize it. Use < > to swap weapons.", x, 512, 16, ui::TEXT_DIM);
    ui::text("Click equipment to cycle through options.", x, 534, 16, ui::TEXT_DIM);

    ui::backHint("BACK");
    if (IsKeyPressed(KEY_ESCAPE)) {
        app.screen = Screen::Lobby;
    }
}
