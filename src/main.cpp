#include "raylib.h"
#include "App.h"
#include "Ui.h"
#include "Sounds.h"

WeaponBuild& App::editedWeapon() {
    Loadout& loadout = profile.loadouts[selectedLoadout];
    return editingPrimary ? loadout.primary : loadout.secondary;
}

int main() {
    // Settings are loaded before the window opens, because anti-aliasing
    // can only be switched on while the window is being created
    Settings settings = loadSettings();
    unsigned int flags = FLAG_WINDOW_RESIZABLE;
    if (settings.antiAliasing) flags |= FLAG_MSAA_4X_HINT;
    if (settings.vsync) flags |= FLAG_VSYNC_HINT;
    SetConfigFlags(flags);
    InitWindow(ui::WIDTH, ui::HEIGHT, GAME_TITLE);
    SetWindowMinSize(640, 360);
    SetExitKey(KEY_NULL);   // ESC goes back a screen instead of closing the game
    applyDisplayMode(settings);
    applyFrameRate(settings);

    ui::loadFont();
    InitAudioDevice();
    SetMasterVolume(settings.volume / 100.0f);
    sfx::load();

    App app;
    app.settings = settings;
    app.antiAliasingAtStart = settings.antiAliasing;
    setupGameData();
    app.profile = makeStartingProfile();
    app.assets.load(settings.highTextures);

    while (!WindowShouldClose() && !app.quit) {
        float dt = GetFrameTime();
        app.time += dt;

        BeginDrawing();
        ui::beginFrame();
        ui::setRenderScale(app.settings.renderScale);

        // Each screen updates and draws itself
        switch (app.screen) {
            case Screen::MainMenu: updateMainMenu(app); break;
            case Screen::Lobby:    updateLobby(app);    break;
            case Screen::Loadouts: updateLoadouts(app); break;
            case Screen::Workshop: updateWorkshop(app); break;
            case Screen::Settings: updateSettings(app); break;
            case Screen::Match:    updateMatch(app);    break;
            case Screen::MatchResults: updateMatchResults(app); break;
        }

        if (app.settings.showFps) {
            ui::text(TextFormat("%d FPS", GetFPS()), 8, 4, 16, ui::OLIVE);
        }

        ui::drawToast(dt);
        EndDrawing();
    }

    saveSettings(app.settings);
    app.assets.unload();
    sfx::unload();
    CloseAudioDevice();
    ui::unloadFont();
    CloseWindow();
    return 0;
}
