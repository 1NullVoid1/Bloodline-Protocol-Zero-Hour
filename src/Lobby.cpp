#include "App.h"
#include "Ui.h"
#include <cmath>

struct Playlist {
    const char* name;
    const char* details;
    bool isNew;
    bool playable = false;   // can you start a match of it yet?
};

// The modes. Only Team Deathmatch is playable so far; the rest show "coming soon".
static const Playlist MULTIPLAYER_MODES[] = {
    { "TEAM DEATHMATCH", "6v6 vs bots  |  First team to 75 kills", false, true },
    { "CRATE RUSH 24/7", "6v6  |  Tiny map, nonstop action", true },
    { "FREE-FOR-ALL",    "8 players  |  Every player for themselves", false },
    { "CAPTURE ZONES",   "6v6  |  Hold the zones to score",  false },
    { "GUN ROTATION",    "8 players  |  New weapon every kill", true },
    { "SNIPERS ONLY",    "6v6  |  One shot, one kill",       false },
};
static const int MULTIPLAYER_MODE_COUNT = 6;

static const Playlist ZOMBIE_MODES[] = {
    { "SURVIVAL",     "1-4 players  |  How many waves can you last?", false },
    { "WAVE DEFENSE", "1-4 players  |  Protect the generator",       false },
};
static const int ZOMBIE_MODE_COUNT = 2;

struct Challenge {
    const char* text;
    int progress;
    int goal;
    int xp;
};

static const Challenge CHALLENGES[] = {
    { "Get 3 kills while sliding", 1, 3, 2000 },
    { "Win 2 matches of Team Deathmatch", 0, 2, 2500 },
    { "Get 10 headshots with an Assault Rifle", 4, 10, 3000 },
};

static void drawChallenges(float x, float y) {
    ui::text("DAILY CHALLENGES", x, y, 20, ui::TEXT);
    ui::textRight("RESETS IN 7H", x + 330, y + 3, 14, ui::TEXT_DIM);
    y += 34;
    for (const Challenge& c : CHALLENGES) {
        Rectangle card = { x, y, 340, 78 };
        ui::panel(card, ui::PANEL);
        ui::text(c.text, x + 14, y + 12, 16, ui::TEXT);
        float barWidth = 170;
        DrawRectangle((int)x + 14, (int)y + 50, (int)barWidth, 3, Fade(ui::TEXT_DIM, 0.3f));
        DrawRectangle((int)x + 14, (int)y + 50, (int)(barWidth * c.progress / c.goal), 3, ui::ACCENT);
        ui::text(TextFormat("%d / %d", c.progress, c.goal), x + 194, y + 43, 16, ui::TEXT_DIM);
        ui::textRight(TextFormat("%d XP", c.xp), x + 330, y + 43, 16, ui::OLIVE);
        y += 86;
    }
}

void updateLobby(App& app) {
    int w = ui::WIDTH;
    int h = ui::HEIGHT;
    bool zombies = (app.mode == GameMode::Zombies);

    // Background: dark with a colored glow (green for zombies)
    Color glow = zombies ? Color{ 70, 140, 60, 255 } : Color{ 60, 90, 140, 255 };
    ui::verticalGradient({ 0, 0, (float)w, (float)h }, { 18, 20, 26, 255 }, { 6, 6, 8, 255 });
    DrawCircleGradient(w / 2, h / 2 + 60, 420, Fade(glow, 0.25f), Fade(glow, 0.0f));

    // The soldier in the middle (drag to spin)
    Rectangle previewArea = { 460, 100, 400, (float)h - 160 };
    handlePreviewDrag(app, previewArea);
    Camera3D camera = {};
    camera.position = { 0, 1.15f, 3.4f };
    camera.target = { 0, 0.95f, 0 };
    camera.up = { 0, 1, 0 };
    camera.fovy = 40;
    camera.projection = CAMERA_PERSPECTIVE;
    ui::begin3D(camera);
    DrawCylinder({ 0.05f, 0, 0 }, 0.6f, 0.6f, 0.01f, 32, Fade(glow, 0.35f));   // floor glow
    Loadout& loadout = app.profile.loadouts[app.selectedLoadout];
    app.assets.drawSoldier(loadout.primary, { 0.05f, 0, 0 }, app.previewYaw, app.time);
    ui::end3D();

    drawTopBar(app, GAME_TITLE, zombies ? "ZOMBIES" : "MULTIPLAYER");

    // Navigation tabs
    ui::tab({ 470, 30, 150, 36 }, "PLAY", true);
    if (ui::tab({ 626, 30, 150, 36 }, "LOADOUTS", false)) {
        app.screen = Screen::Loadouts;
    }

    // Left side: Quick Play and the mode list
    const Playlist* modes = zombies ? ZOMBIE_MODES : MULTIPLAYER_MODES;
    int modeCount = zombies ? ZOMBIE_MODE_COUNT : MULTIPLAYER_MODE_COUNT;
    float x = 40, y = 110;

    Rectangle quickPlay = { x, y, 300, 84 };
    bool hovered = ui::isHovered(quickPlay);
    DrawRectangleGradientH((int)quickPlay.x, (int)quickPlay.y, (int)quickPlay.width, (int)quickPlay.height,
                           Fade(ui::ACCENT, hovered ? 0.55f : 0.35f), Fade(ui::ACCENT, 0.08f));
    ui::outline(quickPlay, 1, ui::ACCENT);
    DrawTriangle({ x + 24, y + 22 }, { x + 24, y + 62 }, { x + 56, y + 42 }, ui::TEXT);
    ui::text("QUICK PLAY", x + 76, y + 28, 32, ui::TEXT);
    if (ui::isClicked(quickPlay)) {
        if (zombies) ui::showToast("Zombies is coming later!");
        else startMatch(app);   // Team Deathmatch is the only mode so far
    }

    Rectangle filters = { x + 310, y, 90, 84 };
    ui::panel(filters, ui::isHovered(filters) ? ui::PANEL_HOVER : ui::PANEL);
    ui::textCentered(TextFormat("%d", modeCount), { filters.x, filters.y + 6, filters.width, 40 }, 34, ui::TEXT);
    ui::textCentered("FILTERS", { filters.x, filters.y + 50, filters.width, 24 }, 14, ui::TEXT_DIM);
    if (ui::isClicked(filters)) {
        ui::showToast("Mode filters will be added once the modes work");
    }

    y += 110;
    ui::sectionHeader(x, y, 400, zombies ? "ZOMBIE MODES" : "FEATURED");
    y += 30;
    for (int i = 0; i < modeCount; i++) {
        Rectangle row = { x, y, 400, 62 };
        if (ui::menuButton(row, modes[i].name, modes[i].details, false, !zombies)) {
            if (modes[i].playable) startMatch(app);
            else ui::showToast(TextFormat("%s isn't playable yet, coming soon!", modes[i].name));
        }
        if (modes[i].isNew) {
            Rectangle tag = { x + 400 - 54, y + 6, 46, 20 };
            DrawRectangleRec(tag, ui::ACCENT);
            ui::textCentered("NEW", tag, 14, ui::BACKGROUND);
        }
        y += 68;
    }

    if (zombies) {
        Rectangle banner = { x, y + 10, 400, 60 };
        ui::panel(banner, Fade(ui::BAD, 0.25f));
        ui::outline(banner, 1, ui::BAD);
        ui::textCentered("ZOMBIES IS COMING LATER", banner, 22, ui::TEXT);
    }

    // Right side: challenges
    drawChallenges((float)w - 380, 110);

    ui::text("Drag to rotate", 600, (float)h - 70, 16, Fade(ui::TEXT_DIM, 0.7f));
    ui::backHint("MAIN MENU");
    if (IsKeyPressed(KEY_ESCAPE)) {
        app.screen = Screen::MainMenu;
    }
}
