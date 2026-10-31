#include "App.h"
#include "Ui.h"
#include <cmath>

// ------------------------------------------------------------
// The settings screen: graphics and controls.
// Every row is a ui::selector (pick from a list with < >) or a
// ui::slider (drag a bar). Changes apply right away and get saved
// to settings.cfg when you leave the screen.
// ------------------------------------------------------------

static const char* onOff(bool value) {
    return value ? "ON" : "OFF";
}

// Step through a list and wrap around at the ends (like the < > weapon buttons)
static int cycle(int index, int direction, int count) {
    return (index + direction + count) % count;
}

// Find where `value` is in a list. If it's not there (say, a hand-edited file), use the last entry.
static int indexOf(const int* list, int count, int value) {
    for (int i = 0; i < count; i++) {
        if (list[i] == value) return i;
    }
    return count - 1;
}

static const char* displayModeName(DisplayMode mode) {
    switch (mode) {
        case DisplayMode::Windowed:   return "WINDOWED";
        case DisplayMode::Borderless: return "BORDERLESS";
        default:                      return "FULLSCREEN";
    }
}

// The help text on the right side shows whatever row the mouse is over
struct Help {
    const char* title = nullptr;
    const char* text = nullptr;
};

static void describe(Help& help, Rectangle row, const char* title, const char* text) {
    if (ui::isHovered(row)) {
        help.title = title;
        help.text = text;
    }
}

static void graphicsTab(App& app, Help& help) {
    Settings& s = app.settings;
    float x = 40, y = 110, width = 640, height = 44, gap = 6;
    auto nextRow = [&]() {
        Rectangle row = { x, y, width, height };
        y += height + gap;
        return row;
    };

    // Quality preset
    Rectangle row = nextRow();
    int step = ui::selector(row, "QUALITY PRESET", presetName(s.preset));
    describe(help, row, "QUALITY PRESET",
             "Sets render scale, anti-aliasing, textures and effects all at once. POTATO is for old laptops "
             "and weak graphics cards. Changing any of those settings yourself switches this to CUSTOM.");
    if (step != 0) {
        int next;
        if (s.preset == PRESET_CUSTOM) next = (step > 0) ? PRESET_POTATO : PRESET_HIGH;
        else next = cycle(s.preset, step, PRESET_CUSTOM);   // CUSTOM isn't in the loop
        bool texturesBefore = s.highTextures;
        applyPreset(s, next);
        if (s.highTextures != texturesBefore) app.assets.reloadCamos(s.highTextures);
    }

    // Display mode
    row = nextRow();
    step = ui::selector(row, "DISPLAY MODE", displayModeName(s.displayMode));
    describe(help, row, "DISPLAY MODE",
             "WINDOWED is a normal window. BORDERLESS fills your screen without changing your monitor's "
             "settings, and is the safest choice. FULLSCREEN takes over the monitor at the resolution below, "
             "which can be a little faster.");
    if (step != 0) {
        s.displayMode = (DisplayMode)cycle((int)s.displayMode, step, 3);
        applyDisplayMode(s);
    }

    // Resolution (borderless always uses the monitor's own resolution)
    row = nextRow();
    bool resolutionMatters = s.displayMode != DisplayMode::Borderless;
    int fits = resolutionsThatFit();
    if (s.resolution >= fits) s.resolution = fits - 1;
    Resolution r = RESOLUTIONS[s.resolution];
    const char* resolutionText = resolutionMatters ? TextFormat("%d x %d", r.width, r.height) : "MONITOR";
    step = ui::selector(row, "RESOLUTION", resolutionText, resolutionMatters);
    describe(help, row, "RESOLUTION",
             "The size of the window, or of the screen in fullscreen. Only sizes that fit your monitor are listed. "
             "To make the game faster, try lowering RENDER SCALE first: it keeps the menus sharp.");
    if (step != 0) {
        s.resolution = cycle(s.resolution, step, fits);
        applyDisplayMode(s);
    }

    // Render scale
    row = nextRow();
    int scaleIndex = indexOf(RENDER_SCALES, RENDER_SCALE_COUNT, s.renderScale);
    step = ui::selector(row, "RENDER SCALE", TextFormat("%d%%", s.renderScale));
    describe(help, row, "RENDER SCALE",
             "Draws the 3D at a lower resolution and stretches it to fit, while the menus and text stay sharp. "
             "This is the biggest speed boost for weak PCs: at 50% the graphics card only draws a quarter "
             "of the pixels.");
    if (step != 0) {
        s.renderScale = RENDER_SCALES[cycle(scaleIndex, step, RENDER_SCALE_COUNT)];
        s.preset = PRESET_CUSTOM;
    }

    // FPS limit
    row = nextRow();
    int fpsIndex = indexOf(FPS_LIMITS, FPS_LIMIT_COUNT, s.fpsLimit);
    step = ui::selector(row, "FRAME RATE LIMIT", s.fpsLimit == 0 ? "UNLIMITED" : TextFormat("%d FPS", s.fpsLimit));
    describe(help, row, "FRAME RATE LIMIT",
             "The most frames per second the game will draw. A lower limit keeps a laptop cooler and quieter. "
             "With VSYNC on, the game also never goes faster than your monitor's refresh rate.");
    if (step != 0) {
        s.fpsLimit = FPS_LIMITS[cycle(fpsIndex, step, FPS_LIMIT_COUNT)];
        applyFrameRate(s);
    }

    // VSync
    row = nextRow();
    step = ui::selector(row, "VSYNC", onOff(s.vsync));
    describe(help, row, "VSYNC",
             "Waits for your monitor before showing each frame, which stops the picture from tearing "
             "when you turn fast. Turn it off for the lowest input delay.");
    if (step != 0) {
        s.vsync = !s.vsync;
        applyFrameRate(s);
    }

    // Anti-aliasing
    row = nextRow();
    step = ui::selector(row, "ANTI-ALIASING", onOff(s.antiAliasing));
    describe(help, row, "ANTI-ALIASING",
             "Smooths the jagged, stair-step edges on 3D objects (4x MSAA). It costs some speed. "
             "This one only changes after you restart the game.");
    if (step != 0) {
        s.antiAliasing = !s.antiAliasing;
        s.preset = PRESET_CUSTOM;
    }
    if (s.antiAliasing != app.antiAliasingAtStart) {
        ui::textRight("RESTART TO APPLY", row.x + row.width - 290, row.y + 14, 16, ui::ACCENT);
    }

    // Texture quality
    row = nextRow();
    step = ui::selector(row, "TEXTURE QUALITY", s.highTextures ? "HIGH" : "LOW");
    describe(help, row, "TEXTURE QUALITY",
             "How detailed the camos are: 128x128 pixels on HIGH, 64x64 on LOW. LOW uses less graphics memory. "
             "This will matter more once the game has real 3D models.");
    if (step != 0) {
        s.highTextures = !s.highTextures;
        s.preset = PRESET_CUSTOM;
        app.assets.reloadCamos(s.highTextures);
    }

    // Menu effects
    row = nextRow();
    step = ui::selector(row, "MENU EFFECTS", onOff(s.menuEffects));
    describe(help, row, "MENU EFFECTS",
             "The drifting smoke behind the main menu. Turning it off saves a little work on very slow PCs.");
    if (step != 0) {
        s.menuEffects = !s.menuEffects;
        s.preset = PRESET_CUSTOM;
    }

    // FPS counter
    row = nextRow();
    step = ui::selector(row, "SHOW FPS", onOff(s.showFps));
    describe(help, row, "SHOW FPS",
             "Shows how many frames per second the game is running at, in the top-left corner. Handy for "
             "testing which settings make the biggest difference on your PC.");
    if (step != 0) {
        s.showFps = !s.showFps;
    }
}

static void controlsTab(App& app, Help& help) {
    Settings& s = app.settings;
    float x = 40, y = 110, width = 640, height = 44;

    // Field of view: the slider works with floats, so round it to a whole number afterwards
    Rectangle row = { x, y, width, height };
    float fov = (float)s.fieldOfView;
    if (ui::slider(row, "FIELD OF VIEW", TextFormat("%d", s.fieldOfView), fov, 60, 120)) {
        s.fieldOfView = (int)roundf(fov);
    }
    describe(help, row, "FIELD OF VIEW",
             "How wide you can see in a match, in degrees from side to side. Higher shows more around you "
             "but makes things look smaller. 90 is a good start.");

    // Mouse sensitivity, rounded to one decimal place
    row.y += height + 6;
    float sensitivity = s.sensitivity;
    if (ui::slider(row, "MOUSE SENSITIVITY", TextFormat("%.1f", s.sensitivity), sensitivity, 0.1f, 5.0f)) {
        s.sensitivity = roundf(sensitivity * 10) / 10;
    }
    describe(help, row, "MOUSE SENSITIVITY",
             "How fast the camera turns when you move the mouse in a match. 1.0 is the default. "
             "At 1.0, moving the mouse 100 pixels turns you 8 degrees.");

    ui::text("Aiming down sights slows the mouse down to match the zoom.", x, row.y + height + 20, 16, ui::TEXT_DIM);
}

static void audioTab(App& app, Help& help) {
    Settings& s = app.settings;
    Rectangle row = { 40, 110, 640, 44 };
    float volume = (float)s.volume;
    if (ui::slider(row, "MASTER VOLUME", TextFormat("%d%%", s.volume), volume, 0, 100)) {
        s.volume = (int)roundf(volume);
        SetMasterVolume(s.volume / 100.0f);
    }
    describe(help, row, "MASTER VOLUME",
             "How loud the whole game is. The sounds are made by code when the game starts, "
             "so there are no sound files.");
}

void updateSettings(App& app) {
    ui::verticalGradient({ 0, 0, (float)ui::WIDTH, (float)ui::HEIGHT }, { 24, 26, 26, 255 }, { 8, 8, 9, 255 });

    drawTopBar(app, GAME_TITLE, "SETTINGS");
    if (ui::tab({ 470, 30, 150, 36 }, "GRAPHICS", app.settingsTab == SettingsTab::Graphics)) {
        app.settingsTab = SettingsTab::Graphics;
    }
    if (ui::tab({ 626, 30, 150, 36 }, "CONTROLS", app.settingsTab == SettingsTab::Controls)) {
        app.settingsTab = SettingsTab::Controls;
    }
    if (ui::tab({ 782, 30, 150, 36 }, "AUDIO", app.settingsTab == SettingsTab::Audio)) {
        app.settingsTab = SettingsTab::Audio;
    }

    Help help;
    if (app.settingsTab == SettingsTab::Graphics) graphicsTab(app, help);
    else if (app.settingsTab == SettingsTab::Controls) controlsTab(app, help);
    else audioTab(app, help);

    // Right side: what the hovered setting does, plus what the PC is doing right now
    Rectangle side = { 720, 110, 520, 494 };
    ui::panel(side, ui::PANEL);
    float textX = side.x + 24, textW = side.width - 48;
    if (help.title != nullptr) {
        ui::text(help.title, textX, side.y + 22, 24, ui::ACCENT);
        ui::paragraph(help.text, textX, side.y + 62, textW, 18, ui::TEXT);
    } else {
        ui::paragraph("Point at a setting to see what it does.", textX, side.y + 22, textW, 18, ui::TEXT_DIM);
    }

    float infoY = side.y + side.height - 120;
    ui::sectionHeader(textX, infoY, textW, "RIGHT NOW");
    int windowW = GetRenderWidth(), windowH = GetRenderHeight();
    int scale = app.settings.renderScale;
    ui::text(TextFormat("Window:  %d x %d", windowW, windowH), textX, infoY + 32, 18, ui::TEXT);
    ui::text(TextFormat("3D drawn at:  %d x %d", windowW * scale / 100, windowH * scale / 100), textX, infoY + 56, 18, ui::TEXT);
    ui::text(TextFormat("Running at:  %d FPS", GetFPS()), textX, infoY + 80, 18, ui::OLIVE);

    ui::backHint("SAVE AND GO BACK");
    if (IsKeyPressed(KEY_ESCAPE)) {
        saveSettings(app.settings);
        app.screen = app.settingsReturnTo;
    }
}
