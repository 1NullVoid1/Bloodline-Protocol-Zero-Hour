#pragma once

#include "raylib.h"

// Small helpers for drawing menus: buttons, tabs, panels, stat bars.
// Every screen uses these, so changing a color here restyles the whole game.
namespace ui {

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

void lockIcon(float x, float y, float size, Color color);
void sectionHeader(float x, float y, float width, const char* label);
void backHint(const char* label);
void verticalGradient(Rectangle r, Color top, Color bottom);

// A short message that pops up at the bottom of the screen
void showToast(const char* message);
void drawToast(float dt);

}
