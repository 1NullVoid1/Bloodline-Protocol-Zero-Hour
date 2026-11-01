#include "App.h"
#include "Match.h"
#include "Ui.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

// ------------------------------------------------------------
// Everything drawn on top of the 3D view during a match (the HUD),
// the pause menu, and the results screen after the match.
// ------------------------------------------------------------

static const Color FRIENDLY = { 110, 170, 240, 255 };
static const Color ENEMY = { 235, 95, 80, 255 };

static Color teamColor(int team) { return team == 0 ? FRIENDLY : ENEMY; }
static Color nameColor(const std::string& name, int team) { return name == "YOU" ? ui::ACCENT : teamColor(team); }

// raylib only draws triangles whose corners go one way around, so fix the order if needed
static void triangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
    float turn = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (turn > 0) std::swap(b, c);
    DrawTriangle(a, b, c, color);
}

// Is a point in front of the camera? (Points behind it would show up mirrored on screen.)
static bool inFront(const Camera3D& camera, Vector3 point) {
    Vector3 forward = Vector3Subtract(camera.target, camera.position);
    return Vector3DotProduct(forward, Vector3Subtract(point, camera.position)) > 0;
}

static std::string clockText(float seconds) {
    int s = (int)ceilf(seconds);
    return TextFormat("%d:%02d", s / 60, s % 60);
}

// ------------------------------------------------------------
// Pieces of the HUD
// ------------------------------------------------------------

static void drawScore(const Match& match) {
    float cx = ui::WIDTH / 2.0f;
    Rectangle mine = { cx - 150, 14, 90, 44 };
    Rectangle theirs = { cx + 60, 14, 90, 44 };
    Rectangle middle = { cx - 60, 14, 120, 44 };
    DrawRectangleRec(mine, Fade(FRIENDLY, 0.75f));
    DrawRectangleRec(theirs, Fade(ENEMY, 0.75f));
    DrawRectangleRec(middle, Fade(BLACK, 0.55f));
    ui::textCentered(TextFormat("%d", match.score[0]), mine, 32, ui::TEXT);
    ui::textCentered(TextFormat("%d", match.score[1]), theirs, 32, ui::TEXT);
    ui::textCentered(clockText(match.timeLeft).c_str(), middle, 24, ui::TEXT);
    ui::textCentered(TextFormat("FIRST TO %d", SCORE_TO_WIN), { cx - 150, 60, 300, 20 }, 14, ui::TEXT);
}

static void drawKillFeed(const Match& match) {
    float right = ui::WIDTH - 20.0f;
    float y = 20;
    for (const KillFeedEntry& k : match.killFeed) {
        float age = match.clock - k.time;
        if (age > 7) continue;
        float alpha = Clamp((7 - age) / 1.0f, 0, 1);
        int size = 18;
        std::string weapon = "[" + k.weapon + (k.headshot ? "  HS]" : "]");
        float wVictim = (float)ui::textWidth(k.victim.c_str(), size);
        float wWeapon = (float)ui::textWidth(weapon.c_str(), size);
        float wKiller = (float)ui::textWidth(k.killer.c_str(), size);
        float total = wVictim + wWeapon + wKiller + 24;
        DrawRectangle((int)(right - total - 10), (int)y - 2, (int)total + 16, 26, Fade(BLACK, 0.4f * alpha));
        ui::textRight(k.victim.c_str(), right, y, size, Fade(nameColor(k.victim, k.victimTeam), alpha));
        ui::textRight(weapon.c_str(), right - wVictim - 12, y, size, Fade(ui::TEXT_DIM, alpha));
        ui::textRight(k.killer.c_str(), right - wVictim - wWeapon - 24, y, size, Fade(nameColor(k.killer, k.killerTeam), alpha));
        y += 28;
    }
}

// The whole map from above. Teammates always show; enemies show for a moment when they shoot.
static void drawMinimap(const Match& match) {
    const Map& map = match.map;
    float scale = 2.8f;   // minimap pixels per meter
    Rectangle area = { 16, 26, map.halfWidth * 2 * scale, map.halfDepth * 2 * scale };
    DrawRectangleRec(area, Fade(BLACK, 0.5f));
    ui::outline(area, 1, Fade(ui::TEXT_DIM, 0.5f));
    auto toMinimap = [&](float x, float z) {
        return Vector2{ area.x + (x + map.halfWidth) * scale, area.y + (z + map.halfDepth) * scale };
    };
    for (const MapBox& box : map.boxes) {
        if (box.bounds.max.y > 4.1f && box.bounds.min.y > 3.5f) continue;   // skip roofs
        Vector2 a = toMinimap(box.bounds.min.x, box.bounds.min.z);
        Vector2 b = toMinimap(box.bounds.max.x, box.bounds.max.z);
        DrawRectangleV(a, { b.x - a.x, b.y - a.y }, Fade(ui::TEXT, 0.35f));
    }
    for (size_t i = 1; i < match.soldiers.size(); i++) {
        const Soldier& s = match.soldiers[i];
        if (!s.alive) continue;
        bool show = s.team == 0 || match.clock - s.lastShot < 1.5f;
        if (!show) continue;
        Vector2 p = toMinimap(s.position.x, s.position.z);
        DrawCircleV(p, 3.5f, teamColor(s.team));
    }
    const Soldier& you = match.soldiers[0];
    if (you.alive) {
        Vector2 p = toMinimap(you.position.x, you.position.z);
        Vector2 dir = { sinf(you.yaw * DEG2RAD), cosf(you.yaw * DEG2RAD) };
        Vector2 side = { -dir.y, dir.x };
        Vector2 tip = { p.x + dir.x * 8, p.y + dir.y * 8 };
        Vector2 left = { p.x - dir.x * 4 + side.x * 5, p.y - dir.y * 4 + side.y * 5 };
        Vector2 rightCorner = { p.x - dir.x * 4 - side.x * 5, p.y - dir.y * 4 - side.y * 5 };
        triangle(tip, left, rightCorner, ui::ACCENT);
    }
}

static void drawNameTags(const Match& match, const Camera3D& camera) {
    const Soldier& you = match.soldiers[0];
    Vector2 center = { ui::WIDTH / 2.0f, ui::HEIGHT / 2.0f };
    for (size_t i = 1; i < match.soldiers.size(); i++) {
        const Soldier& s = match.soldiers[i];
        if (!s.alive) continue;
        Vector3 above = { s.position.x, s.position.y + 2.15f, s.position.z };
        if (!inFront(camera, above)) continue;
        float distance = Vector3Distance(camera.position, above);
        if (distance > 50) continue;
        Vector2 screen = GetWorldToScreenEx(above, camera, ui::WIDTH, ui::HEIGHT);
        bool show;
        if (s.team == you.team) {
            show = match.map.canSee(camera.position, above);   // teammates you can see
        } else {
            // Enemies only when you point at them
            show = Vector2Distance(screen, center) < 60 && match.map.canSee(camera.position, s.eye());
        }
        if (!show) continue;
        int size = 16;
        float w = (float)ui::textWidth(s.name.c_str(), size);
        ui::text(s.name.c_str(), screen.x - w / 2, screen.y - 10, size, teamColor(s.team));
    }
}

static void drawCrosshair(const Match& match, const Camera3D& camera) {
    const Soldier& you = match.soldiers[0];
    const Gun& gun = you.gun();
    float cx = ui::WIDTH / 2.0f, cy = ui::HEIGHT / 2.0f;
    if (usesScope(gun) && match.aimAmount > 0.85f) {
        // Looking through the scope: black around a circle, thin cross lines
        DrawRing({ cx, cy }, 320, 800, 0, 360, 72, BLACK);
        DrawLineEx({ cx - 320, cy }, { cx + 320, cy }, 1.5f, BLACK);
        DrawLineEx({ cx, cy - 320 }, { cx, cy + 320 }, 1.5f, BLACK);
        DrawCircleV({ cx, cy }, 2, { 230, 60, 50, 255 });
        return;
    }
    if (match.aimAmount > 0.6f) {
        // The reticle shows up as the sight reaches your eye. Iron sights don't need one:
        // you line up the top of the front post with the target.
        float fadeIn = Clamp((match.aimAmount - 0.75f) / 0.25f, 0, 1);
        Color red = Fade({ 255, 60, 45, 255 }, fadeIn);
        if (opticLook(gun) == 1) {
            DrawCircleV({ cx, cy }, 4, Fade({ 255, 60, 45, 255 }, 0.25f * fadeIn));   // glow
            DrawCircleV({ cx, cy }, 2, red);
        }
        if (opticLook(gun) == 2) {
            DrawRing({ cx, cy }, 15, 16.5f, 0, 360, 48, red);
            DrawCircleV({ cx, cy }, 1.6f, red);
        }
        return;
    }

    // Four lines that spread apart as your shots get less accurate
    float halfView = tanf(camera.fovy * 0.5f * DEG2RAD);
    float gap = tanf(playerSpread(match) * DEG2RAD) / halfView * (ui::HEIGHT / 2.0f);
    gap = Clamp(gap, 4, 120);
    float length = 9;
    float alpha = 1 - match.aimAmount;
    Color line = Fade(ui::TEXT, alpha);
    Color shadow = Fade(BLACK, 0.5f * alpha);
    Rectangle bars[4] = {
        { cx - gap - length, cy - 1, length, 2 }, { cx + gap, cy - 1, length, 2 },
        { cx - 1, cy - gap - length, 2, length }, { cx - 1, cy + gap, 2, length },
    };
    for (Rectangle b : bars) {
        DrawRectangleRec({ b.x - 1, b.y - 1, b.width + 2, b.height + 2 }, shadow);
        DrawRectangleRec(b, line);
    }
}

static void drawHitMarker(const Match& match) {
    if (match.hitMarkerTimer <= 0) return;
    float cx = ui::WIDTH / 2.0f, cy = ui::HEIGHT / 2.0f;
    Color color = Fade(match.hitMarkerKill ? ENEMY : ui::TEXT, Clamp(match.hitMarkerTimer / 0.2f, 0, 1));
    for (int i = 0; i < 4; i++) {
        float sx = (i % 2 == 0) ? 1.0f : -1.0f;
        float sy = (i < 2) ? 1.0f : -1.0f;
        DrawLineEx({ cx + sx * 7, cy + sy * 7 }, { cx + sx * 15, cy + sy * 15 }, 2.5f, color);
    }
}

static void drawDamage(const Match& match) {
    const Soldier& you = match.soldiers[0];
    // Red around the edges when hurt: stronger at low health and right after a hit
    float strength = Clamp(match.hurtTimer, 0, 0.7f) * 0.6f + (1 - you.health / MAX_HEALTH) * 0.55f;
    if (!you.alive) strength = 0.5f;
    if (strength > 0.01f) {
        Color red = Fade({ 170, 20, 15, 255 }, Clamp(strength, 0, 0.8f));
        Color none = Fade({ 170, 20, 15, 255 }, 0);
        int w = ui::WIDTH, h = ui::HEIGHT, edge = 150;
        DrawRectangleGradientV(0, 0, w, edge, red, none);
        DrawRectangleGradientV(0, h - edge, w, edge, none, red);
        DrawRectangleGradientH(0, 0, edge, h, red, none);
        DrawRectangleGradientH(w - edge, 0, edge, h, none, red);
    }

    // A red arrow pointing toward whoever shot you
    if (match.hurtTimer > 0 && you.alive) {
        Vector3 to = Vector3Subtract(match.hurtFrom, you.position);
        float angle = atan2f(to.x, to.z) * RAD2DEG;
        float relative = fmodf(angle - you.yaw + 540, 360) - 180;   // + means to your left
        float r = relative * DEG2RAD;
        Vector2 dir = { -sinf(r), -cosf(r) };   // up on screen = in front of you
        Vector2 side = { -dir.y, dir.x };
        Vector2 c = { ui::WIDTH / 2.0f + dir.x * 140, ui::HEIGHT / 2.0f + dir.y * 140 };
        Color color = Fade({ 230, 50, 40, 255 }, Clamp(match.hurtTimer / 0.7f, 0, 1));
        triangle({ c.x + dir.x * 22, c.y + dir.y * 22 }, { c.x + side.x * 18, c.y + side.y * 18 },
                 { c.x - side.x * 18, c.y - side.y * 18 }, color);
    }
}

static void drawPlayerStatus(const Match& match) {
    const Soldier& you = match.soldiers[0];
    const Gun& gun = you.gun();

    // Health, bottom-left
    float x = 30, y = ui::HEIGHT - 52.0f;
    float fraction = you.health / MAX_HEALTH;
    Color barColor = fraction > 0.6f ? ui::GOOD : (fraction > 0.3f ? ui::ACCENT : ui::BAD);
    ui::text("HEALTH", x, y - 2, 16, ui::TEXT);
    DrawRectangle((int)x + 70, (int)y + 4, 220, 10, Fade(BLACK, 0.5f));
    DrawRectangle((int)x + 70, (int)y + 4, (int)(220 * fraction), 10, barColor);
    ui::text(TextFormat("%d", (int)ceilf(you.health)), x + 300, y - 2, 16, ui::TEXT);
    ui::text(TEAM_NAMES[0], x, y + 20, 14, FRIENDLY);

    // Ammo, bottom-right
    float right = ui::WIDTH - 30.0f;
    const WeaponDef& weapon = WEAPONS[gun.build.weapon];
    ui::textRight(weapon.name.c_str(), right, ui::HEIGHT - 96.0f, 18, ui::TEXT_DIM);
    Color ammoColor = gun.ammo <= gun.magazineSize / 4 ? ui::BAD : ui::TEXT;
    std::string magazine = TextFormat("/ %d", gun.magazineSize);
    float magazineWidth = (float)ui::textWidth(magazine.c_str(), 20);
    ui::textRight(magazine.c_str(), right, ui::HEIGHT - 52.0f, 20, ui::TEXT_DIM);
    ui::textRight(TextFormat("%d", gun.ammo), right - magazineWidth - 8, ui::HEIGHT - 72.0f, 44, ammoColor);
    ui::textRight("[1] [2] SWITCH", right, ui::HEIGHT - 26.0f, 12, ui::TEXT_DIM);

    // Reload bar / reload hint under the crosshair
    float cx = ui::WIDTH / 2.0f;
    if (gun.reloadTimer > 0) {
        float done = 1 - gun.reloadTimer / reloadTime(gun);
        ui::textCentered("RELOADING", { cx - 100, 400, 200, 20 }, 16, ui::TEXT);
        DrawRectangle((int)cx - 60, 424, 120, 4, Fade(BLACK, 0.5f));
        DrawRectangle((int)cx - 60, 424, (int)(120 * done), 4, ui::ACCENT);
    } else if (gun.ammo <= gun.magazineSize / 4) {
        ui::textCentered(gun.ammo == 0 ? "NO AMMO: PRESS R" : "PRESS R TO RELOAD", { cx - 150, 400, 300, 20 }, 16, ui::ACCENT);
    }
}

static void drawPopups(const Match& match) {
    float y = 450;
    for (const Popup& p : match.popups) {
        float alpha = Clamp(p.life / 0.4f, 0, 1);
        ui::textCentered(p.text.c_str(), { 0, y, (float)ui::WIDTH, 24 }, 20, Fade(p.color, alpha));
        y += 26;
    }
}

static void drawScoreboard(const Match& match, float left, float top, bool dimDead) {
    Rectangle box = { left, top, 700, 470 };
    DrawRectangleRec(box, Fade(BLACK, 0.75f));
    ui::outline(box, 1, Fade(ui::TEXT_DIM, 0.4f));
    ui::text("TEAM DEATHMATCH", box.x + 24, box.y + 16, 22, ui::TEXT);
    ui::textRight(match.map.name, box.x + box.width - 24, box.y + 20, 16, ui::TEXT_DIM);

    float y = box.y + 56;
    for (int team = 0; team < 2; team++) {
        Rectangle header = { box.x + 16, y, box.width - 32, 30 };
        DrawRectangleRec(header, Fade(teamColor(team), 0.35f));
        ui::text(TEAM_NAMES[team], header.x + 10, y + 5, 18, ui::TEXT);
        ui::textRight(TextFormat("%d", match.score[team]), header.x + header.width - 10, y + 4, 20, ui::TEXT);
        ui::textRight("KILLS", header.x + 470, y + 8, 14, ui::TEXT_DIM);
        ui::textRight("DEATHS", header.x + 560, y + 8, 14, ui::TEXT_DIM);
        y += 34;

        // This team's players, most kills first
        std::vector<const Soldier*> players;
        for (const Soldier& s : match.soldiers) if (s.team == team) players.push_back(&s);
        std::sort(players.begin(), players.end(), [](const Soldier* a, const Soldier* b) { return a->kills > b->kills; });
        for (const Soldier* s : players) {
            if (s->isPlayer) DrawRectangle((int)header.x, (int)y, (int)header.width, 26, Fade(ui::ACCENT, 0.18f));
            ui::text(s->name.c_str(), header.x + 10, y + 4, 16, (s->alive || !dimDead) ? ui::TEXT : ui::TEXT_DIM);
            ui::textRight(TextFormat("%d", s->kills), header.x + 470, y + 4, 16, ui::TEXT);
            ui::textRight(TextFormat("%d", s->deaths), header.x + 560, y + 4, 16, ui::TEXT);
            y += 26;
        }
        y += 14;
    }
}

static void drawCenterMessages(const Match& match) {
    const Soldier& you = match.soldiers[0];
    Rectangle full = { 0, 0, (float)ui::WIDTH, (float)ui::HEIGHT };

    if (match.phase == MatchPhase::Countdown) {
        DrawRectangle(0, 200, ui::WIDTH, 220, Fade(BLACK, 0.45f));
        ui::textCentered("TEAM DEATHMATCH", { 0, 220, full.width, 50 }, 44, ui::TEXT);
        ui::textCentered(TextFormat("%s  |  First team to %d kills wins", match.map.name, SCORE_TO_WIN),
                         { 0, 272, full.width, 24 }, 18, ui::TEXT_DIM);
        ui::textCentered("MATCH STARTS IN", { 0, 312, full.width, 24 }, 18, ui::TEXT);
        ui::textCentered(TextFormat("%d", (int)ceilf(match.phaseTimer)), { 0, 336, full.width, 70 }, 64, ui::ACCENT);
    }

    if (match.phase == MatchPhase::Playing && match.clock < COUNTDOWN_LENGTH + 12) {
        ui::textCentered("WASD move   SHIFT sprint   SPACE jump   C crouch   RIGHT MOUSE aim   R reload   TAB scores   ESC pause",
                         { 0, ui::HEIGHT - 140.0f, full.width, 20 }, 14, Fade(ui::TEXT, 0.8f));
    }

    if (!you.alive && match.phase == MatchPhase::Playing) {
        DrawRectangle(0, 250, ui::WIDTH, 120, Fade(BLACK, 0.5f));
        if (you.killedBy >= 0) {
            const Soldier& killer = match.soldiers[you.killedBy];
            ui::textCentered(TextFormat("KILLED BY %s", killer.name.c_str()), { 0, 262, full.width, 40 }, 34, ENEMY);
            ui::textCentered(WEAPONS[killer.gun().build.weapon].name.c_str(), { 0, 302, full.width, 24 }, 18, ui::TEXT_DIM);
        }
        ui::textCentered(TextFormat("RESPAWNING IN %.1f", fmaxf(you.respawnTimer, 0)), { 0, 332, full.width, 24 }, 18, ui::TEXT);
    }

    if (match.phase == MatchPhase::Ended) {
        const char* title = match.winner == 0 ? "VICTORY" : (match.winner == 1 ? "DEFEAT" : "DRAW");
        Color color = match.winner == 0 ? ui::ACCENT : (match.winner == 1 ? ENEMY : ui::TEXT);
        DrawRectangle(0, 230, ui::WIDTH, 170, Fade(BLACK, 0.6f));
        ui::textCentered(title, { 0, 250, full.width, 80 }, 76, color);
        ui::textCentered(TextFormat("%d  -  %d", match.score[0], match.score[1]), { 0, 336, full.width, 40 }, 32, ui::TEXT);
    }
}

void drawMatchHud(App& app) {
    const Match& match = app.match;
    const Soldier& you = match.soldiers[0];
    Camera3D camera = matchCamera(match, app.settings);

    drawNameTags(match, camera);
    if (you.alive) {
        drawCrosshair(match, camera);
        drawHitMarker(match);
    }
    drawDamage(match);
    drawMinimap(match);
    drawScore(match);
    drawKillFeed(match);
    if (you.alive) drawPlayerStatus(match);
    drawPopups(match);
    drawCenterMessages(match);
    if (match.showScoreboard && !match.paused) drawScoreboard(match, 290, 110, true);
}

// ------------------------------------------------------------
// Pause menu
// ------------------------------------------------------------

bool drawPauseMenu(App& app) {
    Match& match = app.match;
    DrawRectangle(0, 0, ui::WIDTH, ui::HEIGHT, Fade(BLACK, 0.6f));
    ui::text("PAUSED", 80, 120, 48, ui::TEXT);
    ui::text(TextFormat("TEAM DEATHMATCH  |  %s", match.map.name), 82, 176, 18, ui::TEXT_DIM);

    float x = 80, y = 230, width = 340, height = 60, gap = 10;
    bool leave = false;
    if (ui::menuButton({ x, y, width, height }, "RESUME", nullptr, false, true)) {
        match.paused = false;
    }
    y += height + gap;
    if (ui::menuButton({ x, y, width, height }, "SETTINGS", nullptr, false, true)) {
        app.settingsReturnTo = Screen::Match;
        app.screen = Screen::Settings;
    }
    y += height + gap;
    if (ui::menuButton({ x, y, width, height }, "LEAVE MATCH", "You keep XP from your kills", false, true)) {
        leave = true;
    }

    // Controls reminder on the right
    float cx = 560, cy = 230;
    ui::sectionHeader(cx, cy, 500, "CONTROLS");
    const char* controls[][2] = {
        { "W A S D", "Move" }, { "MOUSE", "Look" }, { "LEFT MOUSE", "Shoot" }, { "RIGHT MOUSE", "Aim down sights" },
        { "SHIFT", "Sprint" }, { "SPACE", "Jump" }, { "C / CTRL", "Crouch" }, { "R", "Reload" },
        { "1 / 2 / Q / WHEEL", "Switch weapon" }, { "TAB", "Scoreboard" }, { "ESC", "Pause" },
    };
    cy += 34;
    for (auto& c : controls) {
        ui::text(c[0], cx, cy, 16, ui::ACCENT);
        ui::text(c[1], cx + 200, cy, 16, ui::TEXT);
        cy += 26;
    }

    ui::backHint("RESUME");
    return leave;
}

// ------------------------------------------------------------
// Results screen
// ------------------------------------------------------------

static void drawWeaponXp(const WeaponXpResult& r, float x, float y, float width) {
    const WeaponDef& weapon = WEAPONS[r.weapon];
    ui::text(weapon.name.c_str(), x, y, 20, ui::TEXT);
    ui::textRight(TextFormat("+%d XP", r.xp), x + width, y + 2, 18, ui::OLIVE);

    // Level badge and progress toward the next level
    float barY = y + 32;
    ui::text(TextFormat("LV %d", r.levelAfter), x, barY - 4, 16, ui::ACCENT);
    float barX = x + 56, barW = width - 56;
    DrawRectangle((int)barX, (int)barY + 2, (int)barW, 6, Fade(ui::TEXT_DIM, 0.3f));
    float fraction = r.levelAfter >= MAX_WEAPON_LEVEL ? 1.0f : (float)r.xpAfter / xpNeededForLevel(r.levelAfter);
    DrawRectangle((int)barX, (int)barY + 2, (int)(barW * fraction), 6, ui::ACCENT);

    float textY = barY + 18;
    if (r.levelAfter > r.levelBefore) {
        ui::text(TextFormat("LEVEL UP!  %d > %d", r.levelBefore, r.levelAfter), x, textY, 16, ui::ACCENT);
        textY += 22;
    }
    if (!r.unlocks.empty()) {
        std::string list = "UNLOCKED: ";
        int shown = 0;
        for (const std::string& u : r.unlocks) {
            if (shown == 3) {
                list += TextFormat(" +%d more", (int)r.unlocks.size() - 3);
                break;
            }
            if (shown > 0) list += ", ";
            list += u;
            shown++;
        }
        ui::paragraph(list.c_str(), x, textY, width, 15, ui::GOOD);
    }
}

void updateMatchResults(App& app) {
    const Match& match = app.match;
    const Soldier& you = match.soldiers[0];
    Rectangle full = { 0, 0, (float)ui::WIDTH, (float)ui::HEIGHT };
    ui::verticalGradient(full, { 24, 26, 26, 255 }, { 8, 8, 9, 255 });

    const char* title = match.winner == 0 ? "VICTORY" : (match.winner == 1 ? "DEFEAT" : "DRAW");
    Color color = match.winner == 0 ? ui::ACCENT : (match.winner == 1 ? ENEMY : ui::TEXT);
    ui::textCentered(title, { 0, 18, full.width, 70 }, 64, color);
    ui::textCentered(TextFormat("%s  %d  -  %d  %s", TEAM_NAMES[0], match.score[0], match.score[1], TEAM_NAMES[1]),
                     { 0, 86, full.width, 30 }, 22, ui::TEXT);

    // Scoreboard on the left, your stats and weapon XP on the right
    drawScoreboard(match, 40, 130, false);
    Rectangle side = { 770, 130, 470, 470 };
    ui::panel(side, ui::PANEL);
    float x = side.x + 16, y = side.y + 16, w = side.width - 32;
    ui::text("YOUR MATCH", x, y, 18, ui::TEXT_DIM);
    y += 30;
    float kd = you.deaths > 0 ? (float)you.kills / you.deaths : (float)you.kills;
    int accuracy = match.shotsFired > 0 ? match.shotsHit * 100 / match.shotsFired : 0;
    // Stats in two columns to leave room for the XP
    float colW = (w - 30) / 2;
    auto statLine = [&](int index, const char* name, const char* value) {
        float sx = x + (index % 2) * (colW + 30);
        float sy = y + (index / 2) * 24;
        ui::text(name, sx, sy, 16, ui::TEXT);
        ui::textRight(value, sx + colW, sy, 16, ui::TEXT);
    };
    statLine(0, "KILLS", TextFormat("%d", you.kills));
    statLine(1, "DEATHS", TextFormat("%d", you.deaths));
    statLine(2, "K/D", TextFormat("%.2f", kd));
    statLine(3, "HEADSHOTS", TextFormat("%d", match.headshots));
    statLine(4, "ACCURACY", TextFormat("%d%%", accuracy));
    y += 3 * 24 + 12;

    // Player level
    const PlayerXpResult& p = match.playerResult;
    ui::text("PLAYER XP", x, y, 18, ui::TEXT_DIM);
    ui::textRight(TextFormat("+%d XP", p.killXp + p.bonusXp), x + w, y + 2, 18, ui::OLIVE);
    y += 28;
    ui::text(TextFormat("LV %d", p.levelAfter), x, y - 4, 16, ui::ACCENT);
    float barX = x + 56, barW = w - 56;
    DrawRectangle((int)barX, (int)y + 2, (int)barW, 6, Fade(ui::TEXT_DIM, 0.3f));
    float fraction = p.levelAfter >= MAX_PLAYER_LEVEL ? 1.0f : (float)p.xpAfter / xpNeededForPlayerLevel(p.levelAfter);
    DrawRectangle((int)barX, (int)y + 2, (int)(barW * fraction), 6, ui::ACCENT);
    y += 16;
    const char* bonusName = match.winner == 0 ? "Win bonus" : (match.winner == 1 ? "Match bonus" : "Draw bonus");
    ui::text(TextFormat("Kills +%d   %s +%d", p.killXp, bonusName, p.bonusXp), x, y, 15, ui::TEXT_DIM);
    if (p.levelAfter > p.levelBefore) {
        ui::textRight(TextFormat("LEVEL UP!  %d > %d", p.levelBefore, p.levelAfter), x + w, y, 15, ui::ACCENT);
    }
    if (!p.unlocks.empty()) {
        y += 20;
        std::string list = "NEW WEAPON: ";
        for (int i = 0; i < (int)p.unlocks.size(); i++) {
            if (i > 0) list += ", ";
            list += p.unlocks[i];
        }
        ui::text(list.c_str(), x, y, 15, ui::GOOD);
    }
    y += 32;

    ui::text("WEAPON XP", x, y, 18, ui::TEXT_DIM);
    y += 28;
    for (int g = 0; g < 2; g++) {
        if (match.results[g].xp <= 0) continue;
        drawWeaponXp(match.results[g], x, y, w);
        y += 104;
    }

    // Buttons
    if (ui::button({ 40, 620, 200, 44 }, "PLAY AGAIN")) {
        startMatch(app);
        return;
    }
    if (ui::button({ 250, 620, 200, 44 }, "BACK TO LOBBY") || IsKeyPressed(KEY_ESCAPE)) {
        app.screen = Screen::Lobby;
    }
}
