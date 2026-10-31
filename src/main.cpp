#include "raylib.h"
#include "App.h"
#include "Ui.h"

WeaponBuild& App::editedWeapon() {
    Loadout& loadout = profile.loadouts[selectedLoadout];
    return editingPrimary ? loadout.primary : loadout.secondary;
}

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, GAME_TITLE);
    SetExitKey(KEY_NULL);   // ESC goes back a screen instead of closing the game
    SetTargetFPS(60);

    ui::loadFont();

    App app;
    setupGameData();
    app.profile = makeStartingProfile();
    app.assets.load();

    while (!WindowShouldClose() && !app.quit) {
        float dt = GetFrameTime();
        app.time += dt;

        BeginDrawing();
        ClearBackground(ui::BACKGROUND);

        // Each screen updates and draws itself
        switch (app.screen) {
            case Screen::MainMenu: updateMainMenu(app); break;
            case Screen::Lobby:    updateLobby(app);    break;
            case Screen::Loadouts: updateLoadouts(app); break;
            case Screen::Workshop: updateWorkshop(app); break;
        }

        ui::drawToast(dt);
        EndDrawing();
    }

    app.assets.unload();
    ui::unloadFont();
    CloseWindow();
    return 0;
}
