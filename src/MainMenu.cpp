#include "App.h"
#include "Ui.h"
#include <cmath>

// Drifting smoke clouds behind the menu, made of see-through circles
static void drawSmoke(float time) {
    for (int i = 0; i < 14; i++) {
        float speed = 8 + (i % 5) * 4;
        float x = fmodf(i * 173.0f + time * speed, 1600.0f) - 160;
        float y = 120 + (i * 97) % 480 + sinf(time * 0.3f + i) * 20;
        float radius = 140 + (i % 4) * 50;
        DrawCircleGradient((int)x, (int)y, radius, Fade(ui::TEXT_DIM, 0.07f), Fade(ui::TEXT_DIM, 0.0f));
    }
}

void updateMainMenu(App& app) {
    int w = ui::WIDTH;
    int h = ui::HEIGHT;

    // Background: dark gradient with a warm glow on the left
    ui::verticalGradient({ 0, 0, (float)w, (float)h }, { 30, 30, 26, 255 }, { 8, 8, 8, 255 });
    DrawRectangleGradientH(0, 0, 520, h, Fade(ui::ACCENT, 0.18f), Fade(ui::ACCENT, 0.0f));
    if (app.settings.menuEffects) drawSmoke(app.time);

    // Title
    ui::text(GAME_TITLE, 80, 90, 72, ui::TEXT);
    ui::text("PROTOTYPE BUILD", 84, 166, 18, ui::ACCENT);
    DrawRectangle(80, 196, 260, 2, ui::ACCENT_SOFT);

    // The soldier, standing on the right
    Camera3D camera = {};
    camera.position = { 0, 1.3f, 3.6f };
    camera.target = { 0, 1.0f, 0 };
    camera.up = { 0, 1, 0 };
    camera.fovy = 40;
    camera.projection = CAMERA_PERSPECTIVE;
    ui::begin3D(camera);
    Loadout& loadout = app.profile.loadouts[app.selectedLoadout];
    app.assets.drawSoldier(loadout.primary, { 1.3f, 0, 0 }, -18 + sinf(app.time * 0.4f) * 6, app.time);
    ui::end3D();

    // Menu entries
    float x = 80, y = 300, width = 380, height = 64, gap = 10;
    if (ui::menuButton({ x, y, width, height }, "MULTIPLAYER", nullptr, false, true)) {
        app.mode = GameMode::Multiplayer;
        app.screen = Screen::Lobby;
    }
    y += height + gap;
    if (ui::menuButton({ x, y, width, height }, "ZOMBIES", nullptr, false, true)) {
        app.mode = GameMode::Zombies;
        app.screen = Screen::Lobby;
    }
    y += height + gap;
    if (ui::menuButton({ x, y, width, height }, "SETTINGS", nullptr, false, true)) {
        app.settingsReturnTo = Screen::MainMenu;
        app.screen = Screen::Settings;
    }

    if (ui::menuButton({ x, (float)h - 120, width, 52 }, "QUIT TO DESKTOP", nullptr, false, true)) {
        app.quit = true;
    }

    ui::textRight("v0.1", (float)w - 24, (float)h - 30, 16, ui::TEXT_DIM);
}
