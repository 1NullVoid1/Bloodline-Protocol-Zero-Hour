#pragma once

#include "raylib.h"

// Small helpers for drawing menus: buttons, tabs, panels, stat bars.
// Every screen uses these, so changing a color here restyles the whole game.
namespace ui {

// The menus are laid out for a 1280x720 screen. Bigger or smaller windows
// stretch that "virtual screen" to fit (with black bars if the shape is different),
// so every x, y in the menu code is in these 1280x720 "menu pixels".
const int WIDTH = 1280;
const int HEIGHT = 720;

// The color theme
const Color BACKGROUND   = { 12, 14, 13, 255 };
const Color PANEL        = { 22, 26, 24, 215 };
const Color PANEL_HOVER  = { 40, 46, 40, 235 };
const Color PANEL_ACTIVE = { 60, 70, 45, 240 };
const Color ACCENT       = { 222, 178, 82, 255 };    // gold
const Color ACCENT_SOFT  = { 222, 178, 82, 90 };
const Color OLIVE        = { 160, 200, 100, 255 };
const Color TEXT         = { 232, 232, 222, 255 };
const Color TEXT_DIM     = { 150, 152, 140, 255 };
const Color GOOD         = { 120, 210, 100, 255 };
const Color BAD          = { 225, 85, 70, 255 };
const Color LOCKED       = { 95, 95, 92, 255 };

// Call right after BeginDrawing(): sets up the 1280x720 scaling for this frame
void beginFrame();
// Call after EndTextureMode() to go back to drawing on the scaled screen
void restoreCanvas();
// How many real pixels one menu pixel covers (1.5 in a 1920x1080 window)
float pixelScale();
// How far the mouse moved this frame, in menu pixels
Vector2 mouseDelta();

// 3D drawing that follows the scaling. Use these instead of BeginMode3D/EndMode3D.
// When the render scale is below 100%, the 3D is drawn smaller and stretched up,
// which is much less work for the graphics card.
void setRenderScale(int percent);
int renderScalePercent();
void begin3D(Camera3D camera);
void end3D();
// Inside begin3D/end3D: draw the next things on top of everything drawn so far,
// with their own near and far limits. Used for your gun, so it never goes into walls.
void beginCloseUp(Camera3D camera, double nearPlane, double farPlane);

// Only draw inside r until endClip() (like BeginScissorMode, but in menu pixels)
void beginClip(Rectangle r);
void endClip();

bool isHovered(Rectangle r);
bool isClicked(Rectangle r);

// Load the menu font (call once after InitWindow) and free it at the end
void loadFont();
void unloadFont();

int textWidth(const char* text, int size);
void text(const char* text, float x, float y, int size, Color color);
void textCentered(const char* text, Rectangle area, int size, Color color);
void textRight(const char* text, float rightX, float y, int size, Color color);

void panel(Rectangle r, Color color);
void outline(Rectangle r, float thickness, Color color);

// A big menu entry (main menu, mode lists). Returns true when clicked.
bool menuButton(Rectangle r, const char* label, const char* subLabel, bool selected, bool enabled);
// A small rectangular button. Returns true when clicked.
bool button(Rectangle r, const char* label, bool enabled = true);
// A tab at the top of a screen. Returns true when clicked.
bool tab(Rectangle r, const char* label, bool active);

// A stat line with a bar, showing + or - changes in green or red
void statBar(float x, float y, float width, const char* name, int base, int current);

// A settings row: label on the left, "<  value  >" on the right.
// Returns -1 or +1 when an arrow is clicked (clicking the row itself counts as +1), otherwise 0.
int selector(Rectangle r, const char* label, const char* value, bool enabled = true);
// A settings row with a bar you drag. Changes `value` and returns true while dragging.
bool slider(Rectangle r, const char* label, const char* valueText, float& value, float min, float max);
// Text that wraps onto new lines when it reaches `width`. Returns the height it used.
float paragraph(const char* text, float x, float y, float width, int size, Color color);

void lockIcon(float x, float y, float size, Color color);
void sectionHeader(float x, float y, float width, const char* label);
void backHint(const char* label);
void verticalGradient(Rectangle r, Color top, Color bottom);

// A short message that pops up at the bottom of the screen
void showToast(const char* message);
void drawToast(float dt);

}
