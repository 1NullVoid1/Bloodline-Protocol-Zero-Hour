#include "App.h"
#include "Ui.h"

// Title in the top-left with a small "path" above it, and the player badge top-right
void drawTopBar(App& app, const char* breadcrumb, const char* title) {
    ui::text(breadcrumb, 60, 22, 16, ui::TEXT_DIM);
    ui::text("<", 30, 40, 40, ui::TEXT_DIM);
    ui::text(title, 60, 40, 40, ui::TEXT);

    // Player badge
    float right = (float)ui::WIDTH - 30;
    Rectangle badge = { right - 190, 26, 190, 46 };
    ui::panel(badge, ui::PANEL);
    ui::outline(badge, 1, Fade(ui::TEXT_DIM, 0.4f));
    DrawRectangle((int)badge.x + 8, (int)badge.y + 8, 30, 30, ui::ACCENT_SOFT);
    ui::textCentered(TextFormat("%d", app.profile.playerLevel), { badge.x + 8, badge.y + 8, 30, 30 }, 18, ui::TEXT);
    ui::text("PLAYER", badge.x + 50, badge.y + 6, 18, ui::TEXT);
    ui::text(TextFormat("LEVEL %d", app.profile.playerLevel), badge.x + 50, badge.y + 24, 14, ui::TEXT_DIM);

    // Progress toward the next level
    const Profile& p = app.profile;
    float fraction = p.playerLevel >= MAX_PLAYER_LEVEL ? 1.0f : (float)p.playerXp / xpNeededForPlayerLevel(p.playerLevel);
    Rectangle bar = { badge.x + 50, badge.y + 39, badge.width - 62, 3 };
    DrawRectangleRec(bar, Fade(ui::TEXT_DIM, 0.3f));
    DrawRectangleRec({ bar.x, bar.y, bar.width * fraction, bar.height }, ui::ACCENT);
    if (p.playerLevel < MAX_PLAYER_LEVEL) {
        ui::textRight(TextFormat("%d / %d", p.playerXp, xpNeededForPlayerLevel(p.playerLevel)), badge.x + badge.width - 12, badge.y + 25, 12, ui::TEXT_DIM);
    }
}

// Click and drag inside `area` to spin the 3D preview
void handlePreviewDrag(App& app, Rectangle area) {
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && ui::isHovered(area)) {
        app.previewYaw += ui::mouseDelta().x * 0.4f;
    }
}
