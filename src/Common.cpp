#include "App.h"
#include "Ui.h"

// Title in the top-left with a small "path" above it, and the player badge top-right
void drawTopBar(App& app, const char* breadcrumb, const char* title) {
    ui::text(breadcrumb, 60, 22, 16, ui::TEXT_DIM);
    ui::text("<", 30, 40, 40, ui::TEXT_DIM);
    ui::text(title, 60, 40, 40, ui::TEXT);

    // Player badge
    float right = (float)GetScreenWidth() - 30;
    Rectangle badge = { right - 190, 26, 190, 46 };
    ui::panel(badge, ui::PANEL);
    ui::outline(badge, 1, Fade(ui::TEXT_DIM, 0.4f));
    DrawRectangle((int)badge.x + 8, (int)badge.y + 8, 30, 30, ui::ACCENT_SOFT);
    ui::textCentered(TextFormat("%d", app.profile.playerLevel), { badge.x + 8, badge.y + 8, 30, 30 }, 18, ui::TEXT);
    ui::text("PLAYER", badge.x + 50, badge.y + 6, 18, ui::TEXT);
    ui::text(TextFormat("LEVEL %d", app.profile.playerLevel), badge.x + 50, badge.y + 26, 14, ui::TEXT_DIM);
}

// Click and drag inside `area` to spin the 3D preview
void handlePreviewDrag(App& app, Rectangle area) {
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && ui::isHovered(area)) {
        app.previewYaw += GetMouseDelta().x * 0.4f;
    }
}
