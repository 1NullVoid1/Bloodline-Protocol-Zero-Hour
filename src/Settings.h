#pragma once

// Player settings: graphics and controls.
// They're saved to settings.cfg next to game.exe, so they stick between launches.

enum class DisplayMode { Windowed, Borderless, Fullscreen };

// Quality presets set several graphics options at once.
// Changing any one of those options by hand switches the preset to CUSTOM.
enum Preset { PRESET_POTATO, PRESET_LOW, PRESET_MEDIUM, PRESET_HIGH, PRESET_CUSTOM };

struct Resolution {
    int width;
    int height;
};

// The window sizes you can pick (all 16:9, so the menus fit perfectly)
const Resolution RESOLUTIONS[] = {
    { 1280, 720 }, { 1600, 900 }, { 1920, 1080 }, { 2560, 1440 }, { 3840, 2160 },
};
const int RESOLUTION_COUNT = 5;

const int RENDER_SCALES[] = { 50, 67, 75, 85, 100 };   // percent
const int RENDER_SCALE_COUNT = 5;

const int FPS_LIMITS[] = { 30, 60, 120, 144, 240, 0 };  // 0 = unlimited
const int FPS_LIMIT_COUNT = 6;

struct Settings {
    // Graphics
    int preset = PRESET_HIGH;
    DisplayMode displayMode = DisplayMode::Windowed;
    int resolution = 0;          // index into RESOLUTIONS
    int renderScale = 100;       // percent of full resolution the 3D is drawn at
    int fpsLimit = 60;           // 0 = unlimited
    bool vsync = true;
    bool antiAliasing = true;    // smooths jagged edges (needs a restart)
    bool highTextures = true;    // camo textures at 128x128 instead of 64x64
    bool menuEffects = true;     // the drifting smoke on the main menu
    bool showFps = false;

    // Controls
    int fieldOfView = 90;        // degrees, side to side
    float sensitivity = 1.0f;

    // Audio
    int volume = 80;             // percent
};

Settings loadSettings();                 // reads settings.cfg, or returns the defaults
void saveSettings(const Settings& s);    // writes settings.cfg

const char* presetName(int preset);
void applyPreset(Settings& s, int preset);   // fills in the graphics options for a preset

// Apply settings to the running game
void applyDisplayMode(const Settings& s);    // windowed / borderless / fullscreen + resolution
void applyFrameRate(const Settings& s);      // FPS limit and VSync

// Which resolutions fit on this monitor (always at least the first one)
int resolutionsThatFit();
