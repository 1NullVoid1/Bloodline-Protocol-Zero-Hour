#include "Ui.h"
#include "rlgl.h"
#include "raymath.h"
#include <cmath>
#include <string>

namespace ui {

static std::string toastMessage;
static float toastTimer = 0;

// ------------------------------------------------------------
// Scaling the 1280x720 menus to any window size
// ------------------------------------------------------------

static float canvasScale = 1;   // real pixels per menu pixel
static int canvasX = 0;         // where the scaled screen starts inside the window (real pixels)
static int canvasY = 0;
static int canvasW = WIDTH;     // its size in real pixels
static int canvasH = HEIGHT;

void restoreCanvas() {
    rlDrawRenderBatchActive();   // finish anything drawn with the old settings first

    // Only draw inside the scaled screen. OpenGL counts y from the bottom, hence the flip.
    rlViewport(canvasX, GetRenderHeight() - (canvasY + canvasH), canvasW, canvasH);

    // Map menu pixels (0..1280, 0..720) onto that area
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0, WIDTH, HEIGHT, 0, 0, 1);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
}

void beginFrame() {
    int windowW = GetRenderWidth();
    int windowH = GetRenderHeight();

    // Grow the 1280x720 screen as much as fits, keeping its shape
    canvasScale = fminf(windowW / (float)WIDTH, windowH / (float)HEIGHT);
    canvasW = (int)(WIDTH * canvasScale + 0.5f);
    canvasH = (int)(HEIGHT * canvasScale + 0.5f);
    canvasX = (windowW - canvasW) / 2;
    canvasY = (windowH - canvasH) / 2;

    ClearBackground(BLACK);   // the bars around the edge, if the window isn't 16:9
    restoreCanvas();

    // Make GetMousePosition() answer in menu pixels too
    SetMouseOffset(-canvasX, -canvasY);
    SetMouseScale(1 / canvasScale, 1 / canvasScale);

    DrawRectangle(0, 0, WIDTH, HEIGHT, BACKGROUND);
}

float pixelScale() {
    return canvasScale;
}

Vector2 mouseDelta() {
    Vector2 delta = GetMouseDelta();   // raylib gives this in real pixels
    return { delta.x / canvasScale, delta.y / canvasScale };
}

void beginClip(Rectangle r) {
    BeginScissorMode((int)(canvasX + r.x * canvasScale), (int)(canvasY + r.y * canvasScale),
                     (int)(r.width * canvasScale), (int)(r.height * canvasScale));
}

void endClip() {
    EndScissorMode();
}

// ------------------------------------------------------------
// 3D with render scale
// ------------------------------------------------------------

static int renderScale = 100;
static RenderTexture2D lowRes = {};   // where 3D goes when the render scale is below 100%
static bool drawingLowRes = false;

void setRenderScale(int percent) {
    renderScale = percent;
}

int renderScalePercent() {
    return renderScale;
}

void begin3D(Camera3D camera) {
    drawingLowRes = renderScale < 100;

    if (drawingLowRes) {
        // e.g. a 1920x1080 window at 50% draws the 3D at 960x540: a quarter of the pixels
        int w = (int)(canvasW * renderScale / 100.0f);
        int h = (int)(canvasH * renderScale / 100.0f);
        if (w < 64) w = 64;
        if (h < 36) h = 36;
        if (lowRes.id == 0 || lowRes.texture.width != w || lowRes.texture.height != h) {
            if (lowRes.id != 0) UnloadRenderTexture(lowRes);
            lowRes = LoadRenderTexture(w, h);
            SetTextureFilter(lowRes.texture, TEXTURE_FILTER_BILINEAR);   // smooth when stretched
        }
        BeginTextureMode(lowRes);
        ClearBackground(BLANK);   // see-through, so the menu behind shows around the 3D
        // Blend so see-through parts (like the lobby's floor glow) keep their real
        // see-through-ness in the texture, instead of getting faded twice
        rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA,
                                  RL_FUNC_ADD, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        BeginMode3D(camera);
        return;
    }

    // Full resolution: the same as raylib's BeginMode3D, except the picture is always
    // 16:9 like the menus (raylib would use the window's shape, which can be different)
    rlDrawRenderBatchActive();
    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();   // EndMode3D() pops this to get the menu projection back
    rlLoadIdentity();
    double top = rlGetCullDistanceNear() * tan(camera.fovy * 0.5 * DEG2RAD);
    double right = top * WIDTH / HEIGHT;
    rlFrustum(-right, right, -top, top, rlGetCullDistanceNear(), rlGetCullDistanceFar());
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);
    rlMultMatrixf(MatrixToFloat(view));
    rlEnableDepthTest();
}

void end3D() {
    EndMode3D();
    if (drawingLowRes) {
        EndBlendMode();
        EndTextureMode();
        restoreCanvas();
        // Stretch the small picture over the whole screen (render textures are upside down, so flip it).
        // Its colors are already mixed with their see-through-ness, hence "premultiply".
        Rectangle source = { 0, 0, (float)lowRes.texture.width, -(float)lowRes.texture.height };
        BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
        DrawTexturePro(lowRes.texture, source, { 0, 0, (float)WIDTH, (float)HEIGHT }, { 0, 0 }, 0, WHITE);
        EndBlendMode();
        drawingLowRes = false;
    }
}

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

int selector(Rectangle r, const char* label, const char* value, bool enabled) {
    bool hovered = enabled && isHovered(r);
    DrawRectangleRec(r, hovered ? PANEL_HOVER : PANEL);
    if (hovered) DrawRectangle((int)r.x, (int)r.y, 4, (int)r.height, ACCENT);
    text(label, r.x + 20, r.y + (r.height - 20) / 2, 20, enabled ? TEXT : LOCKED);

    // "<  VALUE  >" on the right side of the row
    float arrowW = 32;
    Rectangle left = { r.x + r.width - 270, r.y + 6, arrowW, r.height - 12 };
    Rectangle right = { r.x + r.width - 10 - arrowW, r.y + 6, arrowW, r.height - 12 };
    Rectangle valueArea = { left.x + arrowW, r.y, right.x - left.x - arrowW, r.height };
    textCentered(value, valueArea, 20, enabled ? ACCENT : LOCKED);

    // Draw both arrows before deciding, so neither one flickers on the frame you click
    bool leftClicked = button(left, "<", enabled);
    bool rightClicked = button(right, ">", enabled);
    if (leftClicked) return -1;
    if (rightClicked) return +1;
    if (enabled && isClicked(r)) return +1;
    return 0;
}

bool slider(Rectangle r, const char* label, const char* valueText, float& value, float min, float max) {
    bool hovered = isHovered(r);
    DrawRectangleRec(r, hovered ? PANEL_HOVER : PANEL);
    if (hovered) DrawRectangle((int)r.x, (int)r.y, 4, (int)r.height, ACCENT);
    text(label, r.x + 20, r.y + (r.height - 20) / 2, 20, TEXT);

    Rectangle bar = { r.x + r.width - 270, r.y + r.height / 2 - 3, 180, 6 };
    float t = (value - min) / (max - min);   // 0 at the left end, 1 at the right end
    DrawRectangleRec(bar, Fade(TEXT_DIM, 0.3f));
    DrawRectangle((int)bar.x, (int)bar.y, (int)(bar.width * t), (int)bar.height, ACCENT);
    DrawCircle((int)(bar.x + bar.width * t), (int)(bar.y + bar.height / 2), 9, TEXT);
    textRight(valueText, r.x + r.width - 14, r.y + (r.height - 20) / 2, 20, ACCENT);

    // Hold the mouse anywhere over the bar (with a bit of extra room) to drag it
    Rectangle grabArea = { bar.x - 12, r.y, bar.width + 24, r.height };
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && isHovered(grabArea)) {
        float newT = (GetMousePosition().x - bar.x) / bar.width;
        if (newT < 0) newT = 0;
        if (newT > 1) newT = 1;
        value = min + newT * (max - min);
        return true;
    }
    return false;
}

float paragraph(const char* t, float x, float y, float width, int size, Color color) {
    // Add words to the line one at a time; when the next word won't fit, start a new line
    std::string all = t;
    std::string line;
    float lineY = y;
    size_t start = 0;
    while (start <= all.size()) {
        size_t space = all.find(' ', start);
        if (space == std::string::npos) space = all.size();
        std::string word = all.substr(start, space - start);
        std::string attempt = line.empty() ? word : line + " " + word;
        if (!line.empty() && textWidth(attempt.c_str(), size) > width) {
            text(line.c_str(), x, lineY, size, color);
            lineY += size * 1.35f;
            line = word;
        } else {
            line = attempt;
        }
        start = space + 1;
    }
    if (!line.empty()) {
        text(line.c_str(), x, lineY, size, color);
        lineY += size * 1.35f;
    }
    return lineY - y;
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
    float y = HEIGHT - 44.0f;
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
    Rectangle r = { (WIDTH - width) / 2.0f, HEIGHT - 110.0f, (float)width, 44 };
    DrawRectangleRec(r, Fade(BACKGROUND, 0.9f * alpha));
    DrawRectangleLinesEx(r, 1, Fade(ACCENT, alpha));
    textCentered(toastMessage.c_str(), r, 20, Fade(TEXT, alpha));
}

}
