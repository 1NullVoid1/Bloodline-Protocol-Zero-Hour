#include "Ui.h"
#include <string>

namespace ui {

static std::string toastMessage;
static float toastTimer = 0;

bool isHovered(Rectangle r) {
    return CheckCollisionPointRec(GetMousePosition(), r);
}

bool isClicked(Rectangle r) {
    return isHovered(r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

// ------------------------------------------------------------
// Font: Barlow Semi Condensed (free, SIL Open Font License, see
// assets/fonts/OFL.txt). CMake copies the assets folder next to
// game.exe. If the file is missing we fall back to raylib's font.
// ------------------------------------------------------------
static Font menuFont;
static bool fontLoaded = false;

void loadFont() {
    const char* path = TextFormat("%sassets/fonts/BarlowSemiCondensed-SemiBold.ttf", GetApplicationDirectory());
    if (!FileExists(path)) {
        TraceLog(LOG_WARNING, "Menu font not found at %s, using the default font", path);
        return;
    }
    // Load the letters big, then let mipmaps shrink them smoothly for small text
    menuFont = LoadFontEx(path, 64, nullptr, 0);
    GenTextureMipmaps(&menuFont.texture);
    SetTextureFilter(menuFont.texture, TEXTURE_FILTER_TRILINEAR);
    fontLoaded = true;
}

void unloadFont() {
    if (fontLoaded) UnloadFont(menuFont);
    fontLoaded = false;
}

static Font currentFont() {
    return fontLoaded ? menuFont : GetFontDefault();
}

// A little extra space between letters makes small text easier to read
static float spacingFor(int size) {
    return fontLoaded ? size * 0.03f : size / 10.0f;
}

int textWidth(const char* t, int size) {
    return (int)MeasureTextEx(currentFont(), t, (float)size, spacingFor(size)).x;
}

void text(const char* t, float x, float y, int size, Color color) {
    DrawTextEx(currentFont(), t, { x, y }, (float)size, spacingFor(size), color);
}

void textCentered(const char* t, Rectangle area, int size, Color color) {
    float x = area.x + (area.width - textWidth(t, size)) / 2;
    float y = area.y + (area.height - size) / 2;
    text(t, x, y, size, color);
}

void textRight(const char* t, float rightX, float y, int size, Color color) {
    text(t, rightX - textWidth(t, size), y, size, color);
}

void panel(Rectangle r, Color color) {
    DrawRectangleRec(r, color);
}

void outline(Rectangle r, float thickness, Color color) {
    DrawRectangleLinesEx(r, thickness, color);
}

bool menuButton(Rectangle r, const char* label, const char* subLabel, bool selected, bool enabled) {
    bool hovered = enabled && isHovered(r);
    Color background = selected ? PANEL_ACTIVE : (hovered ? PANEL_HOVER : PANEL);
    DrawRectangleRec(r, background);

    // Gold bar on the left when hovered or selected
    if (hovered || selected) {
        DrawRectangle((int)r.x, (int)r.y, 4, (int)r.height, ACCENT);
    }

    Color labelColor = enabled ? (hovered || selected ? ACCENT : TEXT) : LOCKED;
    bool hasSub = subLabel != nullptr && subLabel[0] != '\0';
    float labelY = hasSub ? r.y + r.height / 2 - 20 : r.y + (r.height - 24) / 2;
    text(label, r.x + 20, labelY, 24, labelColor);
    if (hasSub) {
        text(subLabel, r.x + 20, labelY + 28, 16, enabled ? TEXT_DIM : LOCKED);
    }
    return enabled && isClicked(r);
}

bool button(Rectangle r, const char* label, bool enabled) {
    bool hovered = enabled && isHovered(r);
    DrawRectangleRec(r, hovered ? PANEL_HOVER : PANEL);
    DrawRectangleLinesEx(r, 1, hovered ? ACCENT : Fade(TEXT_DIM, 0.5f));
    textCentered(label, r, 18, enabled ? (hovered ? ACCENT : TEXT) : LOCKED);
    return enabled && isClicked(r);
}

bool tab(Rectangle r, const char* label, bool active) {
    bool hovered = isHovered(r);
    if (active) {
        DrawRectangleRec(r, TEXT);
        textCentered(label, r, 20, BACKGROUND);
    } else {
        DrawRectangleRec(r, hovered ? PANEL_HOVER : PANEL);
        textCentered(label, r, 20, hovered ? ACCENT : TEXT);
    }
    return isClicked(r);
}

void statBar(float x, float y, float width, const char* name, int base, int current) {
    text(name, x, y, 16, TEXT_DIM);
    float barX = x + 110;
    float barWidth = width - 170;
    float barY = y + 7;

    DrawRectangle((int)barX, (int)barY, (int)barWidth, 3, Fade(TEXT_DIM, 0.3f));
    int shown = (current < base) ? current : base;
    DrawRectangle((int)barX, (int)barY, (int)(barWidth * shown / 100.0f), 3, TEXT);
    if (current > base) {
        float start = barX + barWidth * base / 100.0f;
        DrawRectangle((int)start, (int)barY, (int)(barWidth * (current - base) / 100.0f), 3, GOOD);
    } else if (current < base) {
        float start = barX + barWidth * current / 100.0f;
        DrawRectangle((int)start, (int)barY, (int)(barWidth * (base - current) / 100.0f), 3, BAD);
    }

    text(TextFormat("%d", current), barX + barWidth + 12, y, 16, TEXT);
    if (current != base) {
        int diff = current - base;
        text(TextFormat("%+d", diff), barX + barWidth + 40, y, 16, diff > 0 ? GOOD : BAD);
    }
}

void lockIcon(float x, float y, float size, Color color) {
    float bodyH = size * 0.6f;
    DrawRectangle((int)x, (int)(y + size - bodyH), (int)size, (int)bodyH, color);
    DrawRing({ x + size / 2, y + size - bodyH }, size * 0.22f, size * 0.36f, 180, 360, 12, color);
}

void sectionHeader(float x, float y, float width, const char* label) {
    text(label, x, y, 18, TEXT_DIM);
    int w = textWidth(label, 18);
    DrawLine((int)(x + w + 12), (int)(y + 9), (int)(x + width), (int)(y + 9), Fade(TEXT_DIM, 0.4f));
}

void backHint(const char* label) {
    float y = GetScreenHeight() - 44.0f;
    Rectangle key = { 40, y, 40, 26 };
    DrawRectangleRounded(key, 0.3f, 4, Fade(TEXT, 0.15f));
    textCentered("ESC", key, 14, TEXT);
    text(label, 92, y + 4, 18, TEXT);
}

void verticalGradient(Rectangle r, Color top, Color bottom) {
    DrawRectangleGradientV((int)r.x, (int)r.y, (int)r.width, (int)r.height, top, bottom);
}

void showToast(const char* message) {
    toastMessage = message;
    toastTimer = 2.5f;
}

void drawToast(float dt) {
    if (toastTimer <= 0) return;
    toastTimer -= dt;
    float alpha = toastTimer < 0.5f ? toastTimer / 0.5f : 1.0f;
    int width = textWidth(toastMessage.c_str(), 20) + 40;
    Rectangle r = { (GetScreenWidth() - width) / 2.0f, GetScreenHeight() - 110.0f, (float)width, 44 };
    DrawRectangleRec(r, Fade(BACKGROUND, 0.9f * alpha));
    DrawRectangleLinesEx(r, 1, Fade(ACCENT, alpha));
    textCentered(toastMessage.c_str(), r, 20, Fade(TEXT, alpha));
}

}
