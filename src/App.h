#pragma once

#include "Assets.h"
#include "GameData.h"
#include "Match.h"
#include "Settings.h"

// Which screen is showing
enum class Screen { MainMenu, Lobby, Loadouts, Workshop, Settings, Match, MatchResults };

// Multiplayer or zombies, picked on the main menu
enum class GameMode { Multiplayer, Zombies };

// The three tabs at the top of the weapon workshop
enum class WorkshopTab { Attachments, Appearance, Progression };

// The tabs on the settings screen
enum class SettingsTab { Graphics, Controls, Audio };

// Everything the menus need to remember, passed to every screen.
struct App {
    Screen screen = Screen::MainMenu;
    GameMode mode = GameMode::Multiplayer;
    Profile profile;
    Assets assets;
    float time = 0;            // seconds since the game started (for animations)
    bool quit = false;

    // Settings screen
    Settings settings;
    SettingsTab settingsTab = SettingsTab::Graphics;
    Screen settingsReturnTo = Screen::MainMenu;   // where ESC goes from the settings screen
    bool antiAliasingAtStart = true;              // anti-aliasing only changes after a restart

    // Loadouts screen
    int selectedLoadout = 0;

    // Weapon workshop
    bool editingPrimary = true;   // editing the primary or the secondary weapon?
    WorkshopTab workshopTab = WorkshopTab::Attachments;
    int selectedSlot = -1;        // which attachment slot's list is open (-1 = none)
    int selectedCamo = 0;         // camo highlighted on the Appearance tab

    // 3D previews: drag with the mouse to turn them
    float previewYaw = 0;

    // The match being played (Match.h)
    Match match;

    WeaponBuild& editedWeapon();
};

// Each screen has one function that updates and draws it (see Screens.cpp files)
void updateMainMenu(App& app);
void updateLobby(App& app);
void updateLoadouts(App& app);
void updateWorkshop(App& app);
void updateSettings(App& app);

// Shared bits of screen layout
void drawTopBar(App& app, const char* breadcrumb, const char* title);
void handlePreviewDrag(App& app, Rectangle area);
