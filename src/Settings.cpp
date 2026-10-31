#include "Settings.h"
#include "raylib.h"
#include <fstream>
#include <string>

// settings.cfg lives next to game.exe
static std::string settingsPath() {
    return std::string(GetApplicationDirectory()) + "settings.cfg";
}

// ------------------------------------------------------------
// Saving and loading
// The file is plain text, one setting per line, like:
//     renderScale=75
//     vsync=1
// so you can open it in Notepad to see (or fix) what got saved.
// ------------------------------------------------------------

void saveSettings(const Settings& s) {
    std::ofstream file(settingsPath());
    if (!file) {
        TraceLog(LOG_WARNING, "Couldn't save settings to %s", settingsPath().c_str());
        return;
    }
    file << "preset=" << s.preset << "\n";
    file << "displayMode=" << (int)s.displayMode << "\n";
    file << "resolution=" << s.resolution << "\n";
    file << "renderScale=" << s.renderScale << "\n";
    file << "fpsLimit=" << s.fpsLimit << "\n";
    file << "vsync=" << s.vsync << "\n";
    file << "antiAliasing=" << s.antiAliasing << "\n";
    file << "highTextures=" << s.highTextures << "\n";
    file << "menuEffects=" << s.menuEffects << "\n";
    file << "showFps=" << s.showFps << "\n";
    file << "fieldOfView=" << s.fieldOfView << "\n";
    file << "sensitivity=" << s.sensitivity << "\n";
    file << "volume=" << s.volume << "\n";
}

// Keeps a number inside a range, so a hand-edited file can't break the game
static int clampInt(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

Settings loadSettings() {
    Settings s;   // starts with the default values from Settings.h
    std::ifstream file(settingsPath());
    if (!file) {
        return s;   // first launch: no file yet, so use the defaults
    }

    std::string line;
    while (std::getline(file, line)) {
        // Split "name=value" at the '='
        size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        std::string name = line.substr(0, equals);
        std::string value = line.substr(equals + 1);

        // stof/stoi throw an exception on garbage like "abc", so skip bad lines
        try {
            if (name == "preset")            s.preset = clampInt(std::stoi(value), 0, PRESET_CUSTOM);
            else if (name == "displayMode")  s.displayMode = (DisplayMode)clampInt(std::stoi(value), 0, 2);
            else if (name == "resolution")   s.resolution = clampInt(std::stoi(value), 0, RESOLUTION_COUNT - 1);
            else if (name == "renderScale")  s.renderScale = clampInt(std::stoi(value), 25, 100);
            else if (name == "fpsLimit")     s.fpsLimit = clampInt(std::stoi(value), 0, 1000);
            else if (name == "vsync")        s.vsync = std::stoi(value) != 0;
            else if (name == "antiAliasing") s.antiAliasing = std::stoi(value) != 0;
            else if (name == "highTextures") s.highTextures = std::stoi(value) != 0;
            else if (name == "menuEffects")  s.menuEffects = std::stoi(value) != 0;
            else if (name == "showFps")      s.showFps = std::stoi(value) != 0;
            else if (name == "fieldOfView")  s.fieldOfView = clampInt(std::stoi(value), 60, 120);
            else if (name == "sensitivity")  s.sensitivity = std::stof(value);
            else if (name == "volume")       s.volume = clampInt(std::stoi(value), 0, 100);
        } catch (...) {
            TraceLog(LOG_WARNING, "Ignoring bad line in settings.cfg: %s", line.c_str());
        }
    }
    if (s.sensitivity < 0.1f) s.sensitivity = 0.1f;
    if (s.sensitivity > 5.0f) s.sensitivity = 5.0f;
    return s;
}

// ------------------------------------------------------------
// Presets
// ------------------------------------------------------------

const char* presetName(int preset) {
    switch (preset) {
        case PRESET_POTATO: return "POTATO";
        case PRESET_LOW:    return "LOW";
        case PRESET_MEDIUM: return "MEDIUM";
        case PRESET_HIGH:   return "HIGH";
        default:            return "CUSTOM";
    }
}

void applyPreset(Settings& s, int preset) {
    s.preset = preset;
    switch (preset) {
        case PRESET_POTATO:
            s.renderScale = 50;  s.antiAliasing = false; s.highTextures = false; s.menuEffects = false;
            break;
        case PRESET_LOW:
            s.renderScale = 67;  s.antiAliasing = false; s.highTextures = false; s.menuEffects = true;
            break;
        case PRESET_MEDIUM:
            s.renderScale = 85;  s.antiAliasing = true;  s.highTextures = true;  s.menuEffects = true;
            break;
        case PRESET_HIGH:
            s.renderScale = 100; s.antiAliasing = true;  s.highTextures = true;  s.menuEffects = true;
            break;
        default:
            break;   // CUSTOM: leave everything as it is
    }
}

// ------------------------------------------------------------
// Applying to the window
// ------------------------------------------------------------

int resolutionsThatFit() {
    int monitor = GetCurrentMonitor();
    int monitorW = GetMonitorWidth(monitor);
    int monitorH = GetMonitorHeight(monitor);
    int count = 1;   // 1280x720 is always allowed
    for (int i = 1; i < RESOLUTION_COUNT; i++) {
        if (RESOLUTIONS[i].width <= monitorW && RESOLUTIONS[i].height <= monitorH) count = i + 1;
    }
    return count;
}

void applyDisplayMode(const Settings& s) {
    // Step 1: go back to a normal window, whatever mode we were in
    if (IsWindowFullscreen()) ToggleFullscreen();
    if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) ToggleBorderlessWindowed();

    // Step 2: set the size (shrunk to the biggest one that fits this monitor)
    int index = s.resolution;
    if (index >= resolutionsThatFit()) index = resolutionsThatFit() - 1;
    Resolution r = RESOLUTIONS[index];
    SetWindowSize(r.width, r.height);
    int monitor = GetCurrentMonitor();
    SetWindowPosition((GetMonitorWidth(monitor) - r.width) / 2, (GetMonitorHeight(monitor) - r.height) / 2);

    // Step 3: switch into the mode we want
    if (s.displayMode == DisplayMode::Borderless) {
        ToggleBorderlessWindowed();   // fills the monitor at its own resolution
    } else if (s.displayMode == DisplayMode::Fullscreen) {
        ToggleFullscreen();           // takes over the monitor at the chosen resolution
    }
}

void applyFrameRate(const Settings& s) {
    SetTargetFPS(s.fpsLimit);   // 0 means no limit
    if (s.vsync) SetWindowState(FLAG_VSYNC_HINT);
    else ClearWindowState(FLAG_VSYNC_HINT);
}
