#include "App.h"
#include "Ui.h"
#include <algorithm>
#include <cmath>
#include <vector>

// Where each slot's label box sits on screen (Attachments tab)
static const Vector2 SLOT_LABEL_POSITIONS[SLOT_COUNT] = {
    { 1020, 250 },   // muzzle
    { 900, 180 },    // barrel
    { 1010, 450 },   // laser
    { 520, 150 },    // optic
    { 760, 520 },    // underbarrel
    { 470, 540 },    // magazine
    { 420, 270 },    // stock
};
static const float LABEL_WIDTH = 230;
static const float LABEL_HEIGHT = 52;

static Camera3D weaponCamera() {
    Camera3D camera = {};
    camera.position = { 0, 0.15f, 2.4f };
    camera.target = { 0, 0, 0 };
    camera.up = { 0, 1, 0 };
    camera.fovy = 38;
    camera.projection = CAMERA_PERSPECTIVE;
    return camera;
}

static bool isUnlocked(App& app, int weapon, int level) {
    return app.profile.weaponProgress[weapon].level >= level;
}

// ------------------------------------------------------------
// ATTACHMENTS TAB
// ------------------------------------------------------------

static void drawStats(const WeaponBuild& build, const WeaponBuild& shown, float x, float y) {
    const WeaponDef& weapon = WEAPONS[build.weapon];
    Stats base = weapon.base;
    Stats now = statsWithAttachments(shown);

    ui::text(TextFormat("ATTACHMENTS %d/%d", shown.attachmentCount(), MAX_ATTACHMENTS), x, y, 16, ui::TEXT_DIM);
    for (int i = 0; i < MAX_ATTACHMENTS; i++) {
        Color c = i < shown.attachmentCount() ? ui::ACCENT : Fade(ui::TEXT_DIM, 0.4f);
        DrawRectangle((int)(x + 190 + i * 16), (int)(y + 3), 10, 10, c);
    }
    y += 30;
    ui::statBar(x, y, 380, "DAMAGE", base.damage, now.damage);         y += 26;
    ui::statBar(x, y, 380, "RANGE", base.range, now.range);            y += 26;
    ui::statBar(x, y, 380, "ACCURACY", base.accuracy, now.accuracy);   y += 26;
    ui::statBar(x, y, 380, "MOBILITY", base.mobility, now.mobility);   y += 26;
    ui::statBar(x, y, 380, "HANDLING", base.handling, now.handling);   y += 30;

    int mag = magazineWithAttachments(shown);
    ui::text(TextFormat("FIRE RATE  %d RPM", weapon.fireRate), x, y, 16, ui::TEXT);
    ui::text(TextFormat("MAGAZINE  %d", mag), x + 210, y, 16, mag != weapon.magazineSize ? ui::GOOD : ui::TEXT);
}

// The list of attachments for the selected slot. Returns the attachment index
// the mouse is over (so we can preview its stats), or -2 for none.
static int drawAttachmentList(App& app, WeaponBuild& build) {
    const WeaponDef& weapon = WEAPONS[build.weapon];
    int slot = app.selectedSlot;
    int hoveredIndex = -2;

    float x = 40, y = 120, width = 360;
    ui::text(slotName(slot), x, y, 22, ui::ACCENT);
    y += 34;

    // "None" removes whatever is in the slot
    Rectangle noneRow = { x, y, width, 44 };
    if (ui::menuButton(noneRow, "NONE", nullptr, build.attachment[slot] == -1, true)) {
        build.attachment[slot] = -1;
    }
    if (ui::isHovered(noneRow)) hoveredIndex = -1;
    y += 50;

    for (int i = 0; i < (int)weapon.attachments.size(); i++) {
        const Attachment& a = weapon.attachments[i];
        if (a.slot != slot) continue;

        bool unlocked = isUnlocked(app, build.weapon, a.unlockLevel);
        bool equipped = build.attachment[slot] == i;
        Rectangle row = { x, y, width, 56 };
        const char* sub = unlocked ? a.type.c_str() : TextFormat("Unlocks at weapon level %d", a.unlockLevel);
        if (ui::menuButton(row, a.name.c_str(), sub, equipped, unlocked)) {
            bool slotWasEmpty = build.attachment[slot] == -1;
            if (slotWasEmpty && build.attachmentCount() >= MAX_ATTACHMENTS) {
                ui::showToast("You can only use 5 attachments. Remove one first.");
            } else {
                build.attachment[slot] = i;
            }
        }
        if (!unlocked) ui::lockIcon(x + width - 30, y + 18, 16, ui::LOCKED);
        if (equipped) ui::textRight("EQUIPPED", x + width - 12, y + 8, 14, ui::ACCENT);
        if (ui::isHovered(row) && unlocked) hoveredIndex = i;
        y += 62;
    }
    return hoveredIndex;
}

static int hoveredAttachment = -2;   // what the mouse was over last frame (-2 = nothing)

static void attachmentsTab(App& app, WeaponBuild& build) {
    const WeaponDef& weapon = WEAPONS[build.weapon];
    Camera3D camera = weaponCamera();
    Vector3 weaponPos = { 0.05f, 0, 0 };
    float yaw = -12 + app.previewYaw;
    float scale = 0.78f / fmaxf(weapon.bodyLength, 0.7f);   // long weapons are shrunk to fit between the labels

    // If the mouse is over an attachment, show the weapon and stats as if it were equipped
    WeaponBuild shown = build;
    if (app.selectedSlot >= 0 && hoveredAttachment != -2) {
        shown.attachment[app.selectedSlot] = hoveredAttachment;
    }

    // Draw the 3D weapon first so the menus appear on top of it
    handlePreviewDrag(app, { 420, 120, 600, 360 });
    ui::begin3D(camera);
    app.assets.drawWeapon(shown, weaponPos, yaw, scale);
    ui::end3D();

    hoveredAttachment = -2;
    if (app.selectedSlot >= 0) {
        hoveredAttachment = drawAttachmentList(app, build);
    } else {
        ui::text("Click a slot around the weapon", 40, 140, 20, ui::TEXT_DIM);
        ui::text("to see its attachments.", 40, 166, 20, ui::TEXT_DIM);
    }

    // Slot labels with lines pointing at the weapon
    for (int slot = 0; slot < SLOT_COUNT; slot++) {
        Vector2 pos = SLOT_LABEL_POSITIONS[slot];
        Rectangle box = { pos.x, pos.y, LABEL_WIDTH, LABEL_HEIGHT };
        Vector2 target = GetWorldToScreenEx(app.assets.slotAnchor(shown, slot, weaponPos, yaw, scale), camera, ui::WIDTH, ui::HEIGHT);
        Vector2 start = { box.x + (target.x < box.x ? 0 : box.width), box.y + box.height / 2 };
        if (target.x > box.x && target.x < box.x + box.width) {
            start = { box.x + box.width / 2, target.y < box.y ? box.y : box.y + box.height };
        }

        bool selected = app.selectedSlot == slot;
        int index = build.attachment[slot];
        bool hovered = ui::isHovered(box);
        DrawLineEx(start, target, 1, Fade(selected ? ui::ACCENT : ui::TEXT_DIM, 0.7f));
        DrawCircleV(target, 3, selected ? ui::ACCENT : ui::TEXT_DIM);

        ui::panel(box, selected ? ui::PANEL_ACTIVE : (hovered ? ui::PANEL_HOVER : Fade(ui::PANEL, 0.7f)));
        if (index >= 0) DrawRectangle((int)box.x, (int)box.y, 4, (int)box.height, ui::ACCENT);
        if (index >= 0) {
            ui::text(weapon.attachments[index].name.c_str(), box.x + 14, box.y + 8, 16, ui::ACCENT);
            ui::text(weapon.attachments[index].type.c_str(), box.x + 14, box.y + 28, 14, ui::TEXT);
        } else {
            ui::text(TextFormat("+  %s", slotName(slot)), box.x + 14, box.y + 18, 16, ui::TEXT);
        }
        if (ui::isClicked(box)) {
            app.selectedSlot = selected ? -1 : slot;
        }
    }

    drawStats(build, shown, 40, (float)ui::HEIGHT - 250);
}

// ------------------------------------------------------------
// APPEARANCE TAB
// ------------------------------------------------------------

static void appearanceTab(App& app, WeaponBuild& build) {
    const char* categories[] = { "MILITARY", "SPECIAL", "MASTERY" };
    float x = 40, y = 110;
    float swatch = 74, gap = 8;

    for (const char* category : categories) {
        // Count how many in this group are unlocked
        int total = 0, unlocked = 0;
        for (const Camo& c : CAMOS) {
            if (c.category != category) continue;
            total++;
            if (isUnlocked(app, build.weapon, c.unlockLevel)) unlocked++;
        }
        ui::sectionHeader(x, y, 4 * (swatch + gap), TextFormat("%s  [%d/%d]", category, unlocked, total));
        y += 26;

        int column = 0;
        for (int i = 0; i < (int)CAMOS.size(); i++) {
            if (CAMOS[i].category != category) continue;
            Rectangle r = { x + column * (swatch + gap), y, swatch, swatch };
            bool isUnlockedCamo = isUnlocked(app, build.weapon, CAMOS[i].unlockLevel);

            app.assets.drawCamoSwatch(i, r);
            if (!isUnlockedCamo) {
                DrawRectangleRec(r, Fade(BLACK, 0.55f));
                ui::lockIcon(r.x + 6, r.y + 6, 14, ui::TEXT);
            }
            if (build.camo == i) {
                ui::outline(r, 3, ui::ACCENT);
                DrawRectangle((int)r.x + (int)swatch - 20, (int)r.y + 4, 16, 16, ui::ACCENT);
                ui::text("v", r.x + swatch - 16, r.y + 3, 16, ui::BACKGROUND);
            } else if (app.selectedCamo == i) {
                ui::outline(r, 2, ui::TEXT);
            } else if (ui::isHovered(r)) {
                ui::outline(r, 1, ui::TEXT_DIM);
            }

            if (ui::isClicked(r)) {
                app.selectedCamo = i;
                if (isUnlockedCamo) build.camo = i;
            }

            column++;
            if (column == 4) {
                column = 0;
                y += swatch + gap;
            }
        }
        if (column != 0) y += swatch + gap;
        y += 8;
    }

    // Weapon preview wearing the selected camo (even if it's locked, so you can see it)
    WeaponBuild shown = build;
    shown.camo = app.selectedCamo;
    handlePreviewDrag(app, { 420, 110, 820, 380 });
    Camera3D camera = weaponCamera();
    camera.position = { -0.25f, 0.15f, 2.4f };
    camera.target = { -0.25f, 0, 0 };
    ui::begin3D(camera);
    app.assets.drawWeapon(shown, { -0.35f, 0, 0 }, -15 + app.previewYaw, 0.85f / fmaxf(WEAPONS[build.weapon].bodyLength, 0.7f));
    ui::end3D();

    // Info about the selected camo
    const Camo& camo = CAMOS[app.selectedCamo];
    bool camoUnlocked = isUnlocked(app, build.weapon, camo.unlockLevel);
    float infoX = 520, infoY = (float)ui::HEIGHT - 220;
    if (build.camo == app.selectedCamo) {
        Rectangle tag = { infoX, infoY, 100, 22 };
        ui::outline(tag, 1, ui::ACCENT);
        ui::textCentered("EQUIPPED", tag, 14, ui::ACCENT);
    }
    ui::text(camo.name.c_str(), infoX, infoY + 30, 44, ui::TEXT);
    ui::text(camo.category.c_str(), infoX, infoY + 82, 18, ui::TEXT_DIM);
    Color challengeColor = camoUnlocked ? ui::GOOD : ui::TEXT;
    ui::text(TextFormat("%s  %s", camoUnlocked ? "[DONE]" : "[    ]", camo.challenge.c_str()),
             infoX, infoY + 112, 18, challengeColor);
}

// ------------------------------------------------------------
// PROGRESSION TAB
// ------------------------------------------------------------

struct Unlock {
    int level;
    std::string name;
    std::string type;
};

static std::vector<Unlock> unlocksFor(int weaponIndex) {
    std::vector<Unlock> list;
    for (const Attachment& a : WEAPONS[weaponIndex].attachments) {
        list.push_back({ a.unlockLevel, a.name, a.type });
    }
    for (const Camo& c : CAMOS) {
        if (c.unlockLevel > 1) list.push_back({ c.unlockLevel, c.name, "Camo" });
    }
    std::sort(list.begin(), list.end(), [](const Unlock& a, const Unlock& b) { return a.level < b.level; });
    return list;
}

static float unlockScroll = 0;   // how far the unlock row is scrolled

static void progressionTab(App& app, WeaponBuild& build) {
    WeaponProgress& progress = app.profile.weaponProgress[build.weapon];
    std::vector<Unlock> unlocks = unlocksFor(build.weapon);
    int unlockedCount = 0;
    for (const Unlock& u : unlocks) {
        if (u.level <= progress.level) unlockedCount++;
    }

    // Weapon preview
    handlePreviewDrag(app, { 560, 100, 700, 340 });
    Camera3D camera = weaponCamera();
    camera.position = { -0.5f, 0.2f, 2.6f };
    camera.target = { -0.5f, 0, 0 };
    ui::begin3D(camera);
    app.assets.drawWeapon(build, { -0.4f, 0, 0 }, -15 + app.previewYaw, 0.8f / fmaxf(WEAPONS[build.weapon].bodyLength, 0.7f));
    ui::end3D();

    // Level and XP
    float x = 40, y = 120;
    ui::text("WEAPON LEVEL", x, y, 18, ui::TEXT_DIM);
    ui::text(TextFormat("%d", progress.level), x, y + 26, 90, ui::TEXT);
    ui::text(TextFormat("/ %d", MAX_WEAPON_LEVEL), x + 20 + ui::textWidth(TextFormat("%d", progress.level), 90), y + 76, 30, ui::TEXT_DIM);

    float barWidth = 400;
    y += 140;
    DrawRectangle((int)x, (int)y, (int)barWidth, 6, Fade(ui::TEXT_DIM, 0.3f));
    if (progress.level < MAX_WEAPON_LEVEL) {
        float fraction = progress.xp / (float)xpNeededForLevel(progress.level);
        DrawRectangle((int)x, (int)y, (int)(barWidth * fraction), 6, ui::ACCENT);
        ui::text(TextFormat("%d / %d XP to level %d", progress.xp, xpNeededForLevel(progress.level), progress.level + 1),
                 x, y + 14, 16, ui::TEXT_DIM);
    } else {
        DrawRectangle((int)x, (int)y, (int)barWidth, 6, ui::ACCENT);
        ui::text("MAX LEVEL", x, y + 14, 16, ui::ACCENT);
    }

    y += 50;
    ui::text("UNLOCKS", x, y, 18, ui::TEXT_DIM);
    ui::text(TextFormat("%d / %d", unlockedCount, (int)unlocks.size()), x, y + 24, 40, ui::OLIVE);

#ifndef NDEBUG
    // Testing buttons, so you can try the unlocks without playing matches
    y += 90;
    ui::text("TESTING", x, y, 14, ui::TEXT_DIM);
    if (ui::button({ x, y + 20, 130, 36 }, "+1000 XP")) addWeaponXp(progress, 1000);
    if (ui::button({ x + 140, y + 20, 130, 36 }, "MAX LEVEL")) {
        progress.level = MAX_WEAPON_LEVEL;
        progress.xp = 0;
    }
    if (ui::button({ x + 280, y + 20, 120, 36 }, "RESET")) {
        int weaponIndex = build.weapon;
        progress = WeaponProgress();
        build = WeaponBuild();   // locked attachments and camos come off
        build.weapon = weaponIndex;
        app.selectedCamo = 0;
    }
#endif

    // A scrolling row of unlock cards along the bottom
    float cardW = 170, cardH = 120, cardGap = 10;
    float rowY = (float)ui::HEIGHT - 190;
    float rowWidth = (float)ui::WIDTH - 80;
    float maxScroll = std::max(0.0f, unlocks.size() * (cardW + cardGap) - rowWidth);
    unlockScroll -= GetMouseWheelMove() * 60;
    if (ui::button({ 40, rowY - 44, 36, 30 }, "<")) unlockScroll -= (cardW + cardGap) * 3;
    if (ui::button({ 84, rowY - 44, 36, 30 }, ">")) unlockScroll += (cardW + cardGap) * 3;
    unlockScroll = std::clamp(unlockScroll, 0.0f, maxScroll);
    ui::text("Scroll or use the arrows to see every unlock", 132, rowY - 38, 16, ui::TEXT_DIM);

    ui::beginClip({ 40, rowY, rowWidth, cardH });
    for (size_t i = 0; i < unlocks.size(); i++) {
        const Unlock& u = unlocks[i];
        Rectangle card = { 40 + i * (cardW + cardGap) - unlockScroll, rowY, cardW, cardH };
        bool done = u.level <= progress.level;
        ui::panel(card, done ? Color{ 45, 60, 35, 230 } : ui::PANEL);
        DrawRectangle((int)card.x, (int)card.y, (int)card.width, 3, done ? ui::OLIVE : ui::LOCKED);
        ui::text(TextFormat("LEVEL %d", u.level), card.x + 12, card.y + 12, 14, done ? ui::OLIVE : ui::TEXT_DIM);
        ui::text(u.type.c_str(), card.x + 12, card.y + 70, 14, ui::TEXT_DIM);
        ui::text(u.name.c_str(), card.x + 12, card.y + 88, 14, done ? ui::TEXT : ui::LOCKED);
        if (!done) ui::lockIcon(card.x + cardW - 26, card.y + 10, 14, ui::LOCKED);
    }
    ui::endClip();
}

// ------------------------------------------------------------

void updateWorkshop(App& app) {
    int w = ui::WIDTH;
    int h = ui::HEIGHT;
    WeaponBuild& build = app.editedWeapon();
    const WeaponDef& weapon = WEAPONS[build.weapon];
    const WeaponProgress& progress = app.profile.weaponProgress[build.weapon];

    ui::verticalGradient({ 0, 0, (float)w, (float)h }, { 26, 30, 34, 255 }, { 8, 8, 10, 255 });
    DrawCircleGradient(w / 2 + 120, h / 2 - 40, 460, Fade({ 120, 150, 180, 255 }, 0.12f), Fade({ 120, 150, 180, 255 }, 0.0f));

    switch (app.workshopTab) {
        case WorkshopTab::Attachments: attachmentsTab(app, build); break;
        case WorkshopTab::Appearance:  appearanceTab(app, build);  break;
        case WorkshopTab::Progression: progressionTab(app, build); break;
    }

    // Header: weapon name, class and level
    const Loadout& loadout = app.profile.loadouts[app.selectedLoadout];
    drawTopBar(app, TextFormat("LOADOUTS / %s / %s", loadout.name.c_str(), app.editingPrimary ? "PRIMARY" : "SECONDARY"),
               weapon.name.c_str());
    ui::text(TextFormat("%s   |   LEVEL %d", weapon.weaponClass.c_str(), progress.level), 60, 84, 16, ui::OLIVE);

    // Tabs
    if (ui::tab({ 470, 30, 150, 36 }, "ATTACHMENTS", app.workshopTab == WorkshopTab::Attachments)) {
        app.workshopTab = WorkshopTab::Attachments;
    }
    if (ui::tab({ 626, 30, 150, 36 }, "APPEARANCE", app.workshopTab == WorkshopTab::Appearance)) {
        app.workshopTab = WorkshopTab::Appearance;
        app.selectedCamo = build.camo;
    }
    if (ui::tab({ 782, 30, 150, 36 }, "PROGRESSION", app.workshopTab == WorkshopTab::Progression)) {
        app.workshopTab = WorkshopTab::Progression;
    }


    ui::backHint("BACK");
    ui::textRight("Drag the weapon to rotate it", (float)w - 30, (float)h - 40, 16, Fade(ui::TEXT_DIM, 0.7f));
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (app.selectedSlot >= 0) {
            app.selectedSlot = -1;   // first ESC closes the attachment list
        } else {
            app.previewYaw = 0;
            app.screen = Screen::Loadouts;
        }
    }
}
