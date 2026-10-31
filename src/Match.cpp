#include "App.h"
#include "Match.h"
#include "Sounds.h"
#include "Ui.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

// ------------------------------------------------------------
// Team Deathmatch: starting a match, moving, shooting, dying,
// respawning, scoring, and drawing the 3D world.
// The HUD is in MatchHud.cpp and the bots' thinking is in Bots.cpp.
// ------------------------------------------------------------

static const float GRAVITY = 20;          // meters per second, per second
static const float JUMP_SPEED = 6.8f;     // enough to jump onto a crate
static const float STAND_EYE = 1.62f;
static const float CROUCH_EYE = 1.05f;

static float randomFloat(float min, float max) {
    return min + (max - min) * GetRandomValue(0, 10000) / 10000.0f;
}

// Which way someone is looking, as a direction (length 1)
static Vector3 lookDirection(float yaw, float pitch) {
    float y = yaw * DEG2RAD, p = pitch * DEG2RAD;
    return { sinf(y) * cosf(p), sinf(p), cosf(y) * cosf(p) };
}

// ------------------------------------------------------------
// Gun rules
// Every gun works the same way. Its stats (0 to 100, the same numbers
// the workshop shows) are turned into real numbers here, so attachments
// automatically change how a gun behaves in a match.
// ------------------------------------------------------------

static bool isClass(const Gun& gun, const char* weaponClass) {
    return WEAPONS[gun.build.weapon].weaponClass == weaponClass;
}

// Seconds between shots. fireRate is rounds per minute, so 600 RPM = 0.1 seconds
float shotInterval(const Gun& gun) { return 60.0f / WEAPONS[gun.build.weapon].fireRate; }

// Fast guns fire while you hold the button. Slow ones need a click per shot.
bool isAutomatic(const Gun& gun) { return WEAPONS[gun.build.weapon].fireRate >= 500; }

// Better handling = faster reloads and aiming
float reloadTime(const Gun& gun) { return 2.6f - gun.stats.handling * 0.012f; }
float aimTime(const Gun& gun)    { return 0.35f - gun.stats.handling * 0.0025f; }

// How far bullets can stray from the middle of the crosshair, in degrees.
// Better accuracy = smaller. Sniper rifles are terrible without aiming.
float hipSpread(const Gun& gun) {
    float spread = 0.8f + (100 - gun.stats.accuracy) * 0.045f;
    if (isClass(gun, "Sniper Rifle")) spread *= 2.5f;
    return spread;
}

// Walking speed in meters per second. Better mobility = faster.
float moveSpeed(const Gun& gun) { return 4.8f * (0.85f + gun.stats.mobility * 0.003f); }

// Bullets do full damage up to this distance, then get weaker
float effectiveRange(const Gun& gun) {
    if (isClass(gun, "Shotgun")) return 3 + gun.stats.range * 0.25f;
    return 8 + gun.stats.range * 0.4f;
}

// Shotguns fire a spray of pellets
int pelletCount(const Gun& gun) { return isClass(gun, "Shotgun") ? 8 : 1; }

// How much aiming down sights zooms in
float aimZoom(const Gun& gun) { return isClass(gun, "Sniper Rifle") ? 3.5f : 1.35f; }

float damageAt(const Gun& gun, float distance, bool headshot) {
    float damage = (float)gun.stats.damage;
    if (isClass(gun, "Sniper Rifle")) damage *= 1.25f;           // so a body shot kills in one hit
    if (pelletCount(gun) > 1) damage = damage * 1.6f / pelletCount(gun);   // each pellet does a share

    float range = effectiveRange(gun);
    if (distance > range) {
        float past = (distance - range) / range;   // 0 at the edge of the range, 1 at double the range
        damage *= Clamp(1 - past * 0.4f, 0.6f, 1);  // down to 60% damage at most
    }
    if (headshot) damage *= 1.5f;
    return damage;
}

// Lower pitch for bigger guns
static float gunshotPitch(const Gun& gun) {
    if (isClass(gun, "Sniper Rifle")) return 0.6f;
    if (isClass(gun, "Shotgun")) return 0.7f;
    if (isClass(gun, "SMG")) return 1.15f;
    if (isClass(gun, "Pistol")) return 1.1f;
    return 0.95f;
}

// ------------------------------------------------------------
// Bodies
// ------------------------------------------------------------

static float bodyHeight(const Soldier& s) { return s.crouching ? 1.2f : 1.8f; }

// The box used for bumping into walls
static BoundingBox collisionBox(Vector3 feet, float height) {
    const float r = 0.35f;
    return { { feet.x - r, feet.y, feet.z - r }, { feet.x + r, feet.y + height, feet.z + r } };
}

// The boxes bullets can hit: one for the body, a smaller one for the head
static void hitBoxes(const Soldier& s, BoundingBox& body, BoundingBox& head) {
    float h = s.crouching ? 0.68f : 1.0f;
    Vector3 p = s.position;
    body = { { p.x - 0.3f, p.y, p.z - 0.3f }, { p.x + 0.3f, p.y + 1.45f * h, p.z + 0.3f } };
    head = { { p.x - 0.16f, p.y + 1.45f * h, p.z - 0.16f }, { p.x + 0.16f, p.y + 1.82f * h, p.z + 0.16f } };
}

// Moves a soldier and stops them going through walls. Each direction (x, then z,
// then up/down) is tried on its own: if it puts them inside a wall, it's undone.
// That way you slide along walls instead of sticking to them.
void moveSoldier(Match& match, Soldier& s, Vector3 wish, float speed, bool jump, float dt) {
    // Speed up toward the speed we want: quickly on the ground, slowly in the air
    float control = s.onGround ? 12.0f : 2.5f;
    float t = fminf(control * dt, 1);
    s.velocity.x = Lerp(s.velocity.x, wish.x * speed, t);
    s.velocity.z = Lerp(s.velocity.z, wish.z * speed, t);
    if (jump && s.onGround) {
        s.velocity.y = JUMP_SPEED;
        s.onGround = false;
    }
    s.velocity.y -= GRAVITY * dt;

    float height = bodyHeight(s);
    s.position.x += s.velocity.x * dt;
    if (match.map.overlaps(collisionBox(s.position, height))) {
        s.position.x -= s.velocity.x * dt;
        s.velocity.x = 0;
    }
    s.position.z += s.velocity.z * dt;
    if (match.map.overlaps(collisionBox(s.position, height))) {
        s.position.z -= s.velocity.z * dt;
        s.velocity.z = 0;
    }

    s.position.y += s.velocity.y * dt;
    s.onGround = false;
    if (s.position.y <= 0) {
        // Standing on the floor
        s.position.y = 0;
        s.velocity.y = 0;
        s.onGround = true;
    } else if (match.map.overlaps(collisionBox(s.position, height))) {
        if (s.velocity.y < 0) {
            // Fell onto something: stand on top of it
            s.position.y = match.map.highestTop(collisionBox(s.position, height));
            s.onGround = true;
        } else {
            // Bumped your head
            s.position.y -= s.velocity.y * dt;
        }
        s.velocity.y = 0;
    }

    // Walking animation: the legs swing faster the faster you go
    float horizontalSpeed = sqrtf(s.velocity.x * s.velocity.x + s.velocity.z * s.velocity.z);
    s.stridePhase += horizontalSpeed * dt * 2.2f;
    s.stride = s.onGround ? sinf(s.stridePhase) * fminf(horizontalSpeed / 4, 1) : 0.4f;
}

// Soldiers can't stand inside each other: gently push apart anyone too close
static void pushSoldiersApart(Match& match) {
    for (size_t i = 0; i < match.soldiers.size(); i++) {
        for (size_t j = i + 1; j < match.soldiers.size(); j++) {
            Soldier& a = match.soldiers[i];
            Soldier& b = match.soldiers[j];
            if (!a.alive || !b.alive) continue;
            float dx = b.position.x - a.position.x;
            float dz = b.position.z - a.position.z;
            float distance = sqrtf(dx * dx + dz * dz);
            const float minimum = 0.7f;
            if (distance >= minimum || fabsf(a.position.y - b.position.y) > 1.5f) continue;
            if (distance < 0.01f) { dx = 1; dz = 0; distance = 1; }   // exactly on top of each other
            float push = (minimum - distance) / 2;
            Vector3 move = { dx / distance * push, 0, dz / distance * push };
            Vector3 newA = Vector3Subtract(a.position, move);
            Vector3 newB = Vector3Add(b.position, move);
            if (!match.map.overlaps(collisionBox(newA, bodyHeight(a)))) a.position = newA;
            if (!match.map.overlaps(collisionBox(newB, bodyHeight(b)))) b.position = newB;
        }
    }
}

// ------------------------------------------------------------
// Shooting
// ------------------------------------------------------------

// A random direction inside a cone around `forward` (spread = the cone's angle in degrees)
static Vector3 spreadDirection(Vector3 forward, float spreadDegrees) {
    float angle = randomFloat(0, 2 * PI);
    float distance = sqrtf(randomFloat(0, 1)) * tanf(spreadDegrees * DEG2RAD);
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, { 0, 1, 0 }));
    Vector3 up = Vector3CrossProduct(right, forward);
    Vector3 direction = Vector3Add(forward, Vector3Add(Vector3Scale(right, cosf(angle) * distance),
                                                       Vector3Scale(up, sinf(angle) * distance)));
    return Vector3Normalize(direction);
}

// Where the bullet trail starts (roughly the end of the gun)
static Vector3 muzzlePosition(const Match& match, const Soldier& s) {
    float pitch = s.pitch + (s.isPlayer ? match.recoil : 0);
    Vector3 forward = lookDirection(s.yaw, pitch);
    Vector3 right = { -cosf(s.yaw * DEG2RAD), 0, sinf(s.yaw * DEG2RAD) };
    if (s.isPlayer) {
        float side = 0.12f * (1 - match.aimAmount);
        return Vector3Add(s.eye(), Vector3Add(Vector3Scale(forward, 0.5f),
                                              Vector3Add(Vector3Scale(right, side), { 0, -0.1f, 0 })));
    }
    float length = 0.35f + WEAPONS[s.gun().build.weapon].bodyLength * 0.75f;
    Vector3 chest = { s.position.x, s.position.y + 1.33f, s.position.z };
    return Vector3Add(chest, Vector3Add(Vector3Scale(forward, length), Vector3Scale(right, 0.07f)));
}

static void addPuff(Match& match, Vector3 position, Color color, float size) {
    if (match.puffs.size() > 80) match.puffs.erase(match.puffs.begin());
    match.puffs.push_back({ position, color, 0.35f, size });
}

static void addPopup(Match& match, const std::string& text, Color color) {
    if (match.popups.size() >= 4) match.popups.erase(match.popups.begin());
    match.popups.push_back({ text, color, 1.6f });
}

static void endMatch(Match& match) {
    match.phase = MatchPhase::Ended;
    match.phaseTimer = END_SCREEN_DELAY;
    if (match.score[0] > match.score[1]) match.winner = 0;
    else if (match.score[1] > match.score[0]) match.winner = 1;
    else match.winner = -1;
}

static void killSoldier(Match& match, int victim, int attacker, bool headshot) {
    Soldier& v = match.soldiers[victim];
    Soldier& a = match.soldiers[attacker];
    v.alive = false;
    v.health = 0;
    v.diedAt = match.clock;
    v.respawnTimer = RESPAWN_DELAY;
    v.deaths++;
    v.killedBy = attacker;
    a.kills++;
    match.score[a.team]++;

    KillFeedEntry entry;
    entry.killer = a.name;
    entry.victim = v.name;
    entry.weapon = WEAPONS[a.gun().build.weapon].name;
    entry.killerTeam = a.team;
    entry.victimTeam = v.team;
    entry.headshot = headshot;
    entry.time = match.clock;
    match.killFeed.push_back(entry);
    if (match.killFeed.size() > 6) match.killFeed.erase(match.killFeed.begin());

    if (a.isPlayer) {
        // Kills give XP to the gun you're holding
        int xp = headshot ? 150 : 100;
        match.gunKills[a.currentGun]++;
        match.gunXp[a.currentGun] += xp;
        if (headshot) match.headshots++;
        addPopup(match, TextFormat("+%d  %s", xp, headshot ? "HEADSHOT" : "KILL"), ui::ACCENT);
        addPopup(match, "KILLED " + v.name, ui::TEXT);
        match.hitMarkerKill = true;
        sfx::killConfirm();
    }
    if (v.isPlayer) {
        match.aimAmount = 0;
        v.gun().reloadTimer = 0;
    }

    if (match.score[a.team] >= SCORE_TO_WIN) endMatch(match);
}

static void damageSoldier(Match& match, int victim, int attacker, float amount, bool headshot) {
    Soldier& v = match.soldiers[victim];
    if (!v.alive) return;
    v.health -= amount;
    v.lastHurt = match.clock;
    if (match.soldiers[attacker].isPlayer) {
        match.hitMarkerTimer = 0.2f;
        match.hitMarkerKill = false;
        sfx::hitMarker();
    }
    if (v.isPlayer) {
        match.hurtTimer = 0.7f;
        match.hurtFrom = match.soldiers[attacker].position;
        sfx::hurt();
    } else {
        botGotHurt(match, victim, attacker);
    }
    if (v.health <= 0) killSoldier(match, victim, attacker, headshot);
}

// Fires one shot (or one spray of pellets) from where the shooter is looking.
// Bullets are "hitscan": they hit instantly, at the first thing along a straight line.
void fireGun(Match& match, int shooter, float spreadDegrees) {
    Soldier& s = match.soldiers[shooter];
    Gun& gun = s.gun();
    gun.ammo--;
    gun.cooldown = shotInterval(gun);
    s.lastShot = match.clock;

    Vector3 eye = s.eye();
    float pitch = s.pitch + (s.isPlayer ? match.recoil : 0);
    Vector3 forward = lookDirection(s.yaw, pitch);
    Vector3 muzzle = muzzlePosition(match, s);
    int pellets = pelletCount(gun);
    if (pellets > 1) spreadDegrees = fmaxf(spreadDegrees, 4.5f);   // shotguns always spray a bit
    bool hitAnyone = false;

    for (int p = 0; p < pellets; p++) {
        Ray ray = { eye, spreadDirection(forward, spreadDegrees) };
        float distance = 250;
        bool hitWall = match.map.rayHit(ray, distance, distance);

        // Did it hit someone before it hit the wall?
        int victim = -1;
        bool headshot = false;
        for (int i = 0; i < (int)match.soldiers.size(); i++) {
            const Soldier& other = match.soldiers[i];
            if (!other.alive || other.team == s.team) continue;   // no friendly fire
            BoundingBox body, head;
            hitBoxes(other, body, head);
            RayCollision c = GetRayCollisionBox(ray, head);
            if (c.hit && c.distance < distance) {
                distance = c.distance;
                victim = i;
                headshot = true;
            }
            c = GetRayCollisionBox(ray, body);
            if (c.hit && c.distance < distance) {
                distance = c.distance;
                victim = i;
                headshot = false;
            }
        }

        Vector3 end = Vector3Add(ray.position, Vector3Scale(ray.direction, distance));
        if (victim >= 0) {
            hitAnyone = true;
            addPuff(match, end, { 190, 40, 35, 255 }, 0.09f);
            damageSoldier(match, victim, shooter, damageAt(gun, distance, headshot), headshot);
        } else if (hitWall) {
            addPuff(match, end, { 150, 140, 122, 255 }, 0.12f);
        }
        if (match.tracers.size() > 60) match.tracers.erase(match.tracers.begin());
        match.tracers.push_back({ muzzle, end, 0.06f });
    }

    // Sound: your own shots are loud and in both ears. Bots' shots get quieter
    // with distance and come from their side (pan).
    const Soldier& you = match.soldiers[0];
    if (s.isPlayer) {
        match.shotsFired++;
        if (hitAnyone) match.shotsHit++;
        sfx::gunshot(gunshotPitch(gun) * randomFloat(0.97f, 1.03f), 0.55f, 0.5f);
    } else {
        Vector3 toShooter = Vector3Subtract(s.position, you.position);
        float distance = Vector3Length(toShooter);
        float volume = 0.5f * Clamp(1 - distance / 70, 0.06f, 1);
        Vector3 yourRight = { -cosf(you.yaw * DEG2RAD), 0, sinf(you.yaw * DEG2RAD) };
        float side = distance > 0.1f ? Vector3DotProduct(Vector3Scale(toShooter, 1 / distance), yourRight) : 0;
        sfx::gunshot(gunshotPitch(gun) * randomFloat(0.95f, 1.05f), volume, 0.5f - side * 0.4f);
    }
    botHeardShot(match, shooter);
}

// ------------------------------------------------------------
// Spawning
// ------------------------------------------------------------

static Gun makeGun(const WeaponBuild& build) {
    Gun gun;
    gun.build = build;
    gun.stats = statsWithAttachments(build);
    gun.magazineSize = magazineWithAttachments(build);
    gun.ammo = gun.magazineSize;
    return gun;
}

// Picks the spawn point furthest from enemies. Your own side's spawns get a bonus,
// but if enemies are camping there you may spawn on the other side instead.
static Vector3 pickSpawn(const Match& match, int team) {
    Vector3 best = match.map.spawns[team][0];
    float bestScore = -1e9f;
    for (int side = 0; side < 2; side++) {
        for (Vector3 spawn : match.map.spawns[side]) {
            float closestEnemy = 60;
            bool occupied = false;
            for (const Soldier& s : match.soldiers) {
                if (!s.alive) continue;
                float d = Vector3Distance(s.position, spawn);
                if (d < 1.2f) occupied = true;
                if (s.team != team && d < closestEnemy) closestEnemy = d;
            }
            if (occupied) continue;
            float score = closestEnemy + (side == team ? 15 : 0) + randomFloat(0, 6);
            if (score > bestScore) {
                bestScore = score;
                best = spawn;
            }
        }
    }
    return best;
}

static void spawnSoldier(Match& match, int index, Vector3 position) {
    Soldier& s = match.soldiers[index];
    s.position = position;
    s.velocity = { 0, 0, 0 };
    s.yaw = (s.team == 0) ? 90.0f : -90.0f;   // face the middle of the map
    s.pitch = 0;
    s.alive = true;
    s.health = MAX_HEALTH;
    s.crouching = false;
    s.eyeHeight = STAND_EYE;
    s.onGround = true;
    s.currentGun = 0;
    for (Gun& gun : s.guns) {
        gun.ammo = gun.magazineSize;
        gun.cooldown = 0;
        gun.reloadTimer = 0;
    }
    s.brain = BotBrain();
    s.brain.thinkTimer = randomFloat(0, 0.3f);
}

// A random weapon for a bot, with a random camo and a couple of attachments
static WeaponBuild randomBotWeapon() {
    // Bots mostly use primaries, sometimes snipers, never just a pistol
    std::vector<int> choices;
    for (int i = 0; i < (int)WEAPONS.size(); i++) {
        if (WEAPONS[i].weaponClass == "Pistol") continue;
        int copies = WEAPONS[i].isPrimary ? 3 : 1;
        for (int c = 0; c < copies; c++) choices.push_back(i);
    }
    WeaponBuild build;
    build.weapon = choices[GetRandomValue(0, (int)choices.size() - 1)];
    build.camo = GetRandomValue(0, std::min(7, (int)CAMOS.size() - 1));
    const WeaponDef& weapon = WEAPONS[build.weapon];
    for (int tries = 0; tries < 2 && !weapon.attachments.empty(); tries++) {
        int a = GetRandomValue(0, (int)weapon.attachments.size() - 1);
        if (weapon.attachments[a].unlockLevel <= 12) build.attachment[weapon.attachments[a].slot] = a;
    }
    return build;
}

static const char* BOT_NAMES[] = {
    "Brix", "Tanner", "Okafor", "Marlow", "Kestrel", "Juno", "Rourke", "Castillo",
    "Ivers", "Delgado", "Haldane", "Sable", "Quill", "Fennick", "Osei", "Whitlock",
};
static const int BOT_NAME_COUNT = 16;

void startMatch(App& app) {
    Match& match = app.match;
    match = Match();   // a fresh match: scores, effects and stats all back to zero
    match.map.buildKilnYard();

    // Shuffle the bot names
    std::vector<int> names;
    for (int i = 0; i < BOT_NAME_COUNT; i++) names.push_back(i);
    for (int i = BOT_NAME_COUNT - 1; i > 0; i--) std::swap(names[i], names[GetRandomValue(0, i)]);

    const Loadout& loadout = app.profile.loadouts[app.selectedLoadout];
    int nextName = 0;
    for (int team = 0; team < 2; team++) {
        for (int i = 0; i < TEAM_SIZE; i++) {
            Soldier s;
            s.team = team;
            s.isPlayer = (team == 0 && i == 0);
            if (s.isPlayer) {
                s.name = "YOU";
                s.guns[0] = makeGun(loadout.primary);
                s.guns[1] = makeGun(loadout.secondary);
            } else {
                s.name = BOT_NAMES[names[nextName++]];
                s.guns[0] = makeGun(randomBotWeapon());
                s.guns[1] = s.guns[0];
            }
            match.soldiers.push_back(s);
            spawnSoldier(match, (int)match.soldiers.size() - 1, match.map.spawns[team][i]);
        }
    }
    setupBots(match);

    app.screen = Screen::Match;
    DisableCursor();              // hide the mouse and lock it to the window
    match.mouseSkipFrames = 3;    // the first mouse move after locking can be a big jump
}

// ------------------------------------------------------------
// The player
// ------------------------------------------------------------

static void startReload(Gun& gun) {
    if (gun.reloadTimer > 0 || gun.ammo >= gun.magazineSize) return;
    gun.reloadTimer = reloadTime(gun);
    sfx::reloadStart();
}

// Reloads and shot timers count down for everyone
static void updateGunTimers(Soldier& s, float dt) {
    Gun& gun = s.gun();
    gun.cooldown -= dt;
    if (gun.reloadTimer > 0) {
        gun.reloadTimer -= dt;
        if (gun.reloadTimer <= 0) {
            gun.reloadTimer = 0;
            gun.ammo = gun.magazineSize;
            if (s.isPlayer) sfx::reloadEnd();
        }
    }
}

// The spread of your shots right now (hip fire, aiming, moving and shooting all change it)
float playerSpread(const Match& match) {
    const Soldier& you = match.soldiers[0];
    const Gun& gun = you.gun();
    float hip = hipSpread(gun) * (1 + match.spreadBloom * 0.6f);
    float speed = sqrtf(you.velocity.x * you.velocity.x + you.velocity.z * you.velocity.z);
    if (speed > 1) hip *= 1.3f;
    if (!you.onGround) hip *= 1.6f;
    if (you.crouching) hip *= 0.85f;
    float aimed = isClass(gun, "Sniper Rifle") ? 0.0f : hip * 0.12f + match.spreadBloom * 0.3f;
    return Lerp(hip, aimed, match.aimAmount);
}

static void updatePlayer(App& app, float dt, bool canAct) {
    Match& match = app.match;
    Soldier& you = match.soldiers[0];
    if (!you.alive) return;
    Gun& gun = you.gun();

    // Looking around. Aiming down sights slows the mouse to match the zoom.
    Vector2 mouse = GetMouseDelta();
    if (match.mouseSkipFrames > 0) {
        mouse = { 0, 0 };
        match.mouseSkipFrames--;
    }
    float zoom = Lerp(1, aimZoom(gun), match.aimAmount);
    float degreesPerPixel = 0.08f * app.settings.sensitivity / zoom;
    you.yaw -= mouse.x * degreesPerPixel;
    you.pitch -= mouse.y * degreesPerPixel;
    you.pitch = Clamp(you.pitch, -88, 88);
    if (you.yaw > 180) you.yaw -= 360;
    if (you.yaw < -180) you.yaw += 360;

    if (!canAct) {
        moveSoldier(match, you, { 0, 0, 0 }, 0, false, dt);
        return;
    }

    // Moving: forward/back and left/right, relative to where you look
    float yaw = you.yaw * DEG2RAD;
    Vector3 forward = { sinf(yaw), 0, cosf(yaw) };
    Vector3 right = { -cosf(yaw), 0, sinf(yaw) };
    Vector3 wish = { 0, 0, 0 };
    if (IsKeyDown(KEY_W)) wish = Vector3Add(wish, forward);
    if (IsKeyDown(KEY_S)) wish = Vector3Subtract(wish, forward);
    if (IsKeyDown(KEY_D)) wish = Vector3Add(wish, right);
    if (IsKeyDown(KEY_A)) wish = Vector3Subtract(wish, right);
    if (Vector3Length(wish) > 0) wish = Vector3Normalize(wish);

    bool wantsToShoot = isAutomatic(gun) ? IsMouseButtonDown(MOUSE_BUTTON_LEFT) : IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool sprinting = IsKeyDown(KEY_LEFT_SHIFT) && IsKeyDown(KEY_W) && match.aimAmount < 0.3f && !wantsToShoot;
    match.sprinting = sprinting;

    // Crouch (C or Ctrl). Sprinting stands you back up.
    if (IsKeyPressed(KEY_C) || IsKeyPressed(KEY_LEFT_CONTROL)) {
        if (!you.crouching) you.crouching = true;
        else if (!match.map.overlaps(collisionBox(you.position, 1.8f))) you.crouching = false;   // room to stand?
    }
    if (sprinting && you.crouching && !match.map.overlaps(collisionBox(you.position, 1.8f))) you.crouching = false;
    float targetEye = you.crouching ? CROUCH_EYE : STAND_EYE;
    you.eyeHeight = Lerp(you.eyeHeight, targetEye, fminf(12 * dt, 1));

    float speed = moveSpeed(gun);
    if (sprinting) speed *= 1.45f;
    if (you.crouching) speed *= 0.55f;
    speed *= Lerp(1, 0.6f, match.aimAmount);
    moveSoldier(match, you, wish, speed, IsKeyPressed(KEY_SPACE), dt);

    // Aim down sights (hold right mouse)
    bool wantsToAim = IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !sprinting;
    float aimStep = dt / aimTime(gun);
    match.aimAmount = Clamp(match.aimAmount + (wantsToAim ? aimStep : -aimStep), 0, 1);

    // Switch guns: 1, 2, Q or the mouse wheel
    int switchTo = you.currentGun;
    if (IsKeyPressed(KEY_ONE)) switchTo = 0;
    if (IsKeyPressed(KEY_TWO)) switchTo = 1;
    if (IsKeyPressed(KEY_Q) || GetMouseWheelMove() != 0) switchTo = 1 - you.currentGun;
    if (switchTo != you.currentGun) {
        you.gun().reloadTimer = 0;          // switching cancels a reload
        you.currentGun = switchTo;
        you.gun().cooldown = 0.35f;         // time to raise the other gun
        match.aimAmount = 0;
        match.spreadBloom = 0;
    }
    Gun& held = you.gun();

    // Reload
    if (IsKeyPressed(KEY_R)) startReload(held);

    // Shoot
    if (wantsToShoot && !sprinting && held.cooldown <= 0 && held.reloadTimer <= 0) {
        if (held.ammo > 0) {
            fireGun(match, 0, playerSpread(match));
            // Recoil kicks the view up; it settles back by itself
            float kick = 0.45f + (100 - held.stats.accuracy) * 0.02f;
            if (isClass(held, "Sniper Rifle") || isClass(held, "Shotgun")) kick *= 3;
            match.recoil = fminf(match.recoil + kick * Lerp(1, 0.7f, match.aimAmount), 8);
            match.spreadBloom = fminf(match.spreadBloom + 0.15f, 1);
        } else {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) sfx::dryFire();
            startReload(held);
        }
    }
}

// ------------------------------------------------------------
// The match loop
// ------------------------------------------------------------

static void updateEffects(Match& match, float dt) {
    for (Tracer& t : match.tracers) t.life -= dt;
    for (Puff& p : match.puffs) {
        p.life -= dt;
        p.size += dt * 0.3f;
        p.position.y += dt * 0.4f;
    }
    for (Popup& p : match.popups) p.life -= dt;
    match.tracers.erase(std::remove_if(match.tracers.begin(), match.tracers.end(), [](const Tracer& t) { return t.life <= 0; }), match.tracers.end());
    match.puffs.erase(std::remove_if(match.puffs.begin(), match.puffs.end(), [](const Puff& p) { return p.life <= 0; }), match.puffs.end());
    match.popups.erase(std::remove_if(match.popups.begin(), match.popups.end(), [](const Popup& p) { return p.life <= 0; }), match.popups.end());

    match.hitMarkerTimer -= dt;
    match.hurtTimer -= dt;
    match.recoil -= match.recoil * fminf(7 * dt, 1);
    match.spreadBloom = fmaxf(match.spreadBloom - dt * 2.5f, 0);
    match.swayTime += dt;
}

static void simulate(App& app, float dt) {
    Match& match = app.match;
    match.clock += dt;
    updateEffects(match, dt);

    if (match.phase == MatchPhase::Countdown) {
        updatePlayer(app, dt, false);   // you can look around but not move yet
        match.phaseTimer -= dt;
        if (match.phaseTimer <= 0) match.phase = MatchPhase::Playing;
        return;
    }
    if (match.phase == MatchPhase::Ended) {
        match.phaseTimer -= dt;
        if (match.phaseTimer <= 0) finishMatch(app);
        return;
    }

    match.timeLeft -= dt;
    if (match.timeLeft <= 0) {
        match.timeLeft = 0;
        endMatch(match);
        return;
    }

    updatePlayer(app, dt, true);
    for (int i = 1; i < (int)match.soldiers.size(); i++) {
        updateBot(match, i, dt);
        if (match.phase != MatchPhase::Playing) return;   // that shot won the match
    }
    for (Soldier& s : match.soldiers) {
        if (s.alive) updateGunTimers(s, dt);
    }
    pushSoldiersApart(match);

    for (int i = 0; i < (int)match.soldiers.size(); i++) {
        Soldier& s = match.soldiers[i];
        if (s.alive) {
            // Health comes back 4 seconds after you last got hit
            if (match.clock - s.lastHurt > 4) s.health = fminf(s.health + 40 * dt, MAX_HEALTH);
        } else {
            s.respawnTimer -= dt;
            if (s.respawnTimer <= 0) spawnSoldier(match, i, pickSpawn(match, s.team));
        }
    }
}

// ------------------------------------------------------------
// Camera and drawing
// ------------------------------------------------------------

Camera3D matchCamera(const Match& match, const Settings& settings) {
    const Soldier& you = match.soldiers[0];
    Camera3D camera = {};
    camera.up = { 0, 1, 0 };
    camera.projection = CAMERA_PERSPECTIVE;

    // The FOV setting is the horizontal angle (like most PC shooters).
    // raylib wants the vertical angle, so convert it for a 16:9 screen.
    float horizontal = settings.fieldOfView * DEG2RAD;
    float vertical = 2 * atanf(tanf(horizontal / 2) * 9.0f / 16.0f);
    float zoom = Lerp(1, aimZoom(you.gun()), match.aimAmount);
    camera.fovy = 2 * atanf(tanf(vertical / 2) / zoom) * RAD2DEG;

    if (you.alive) {
        camera.position = you.eye();
        // A little bob while walking (less when aiming)
        float bob = 1 - match.aimAmount;
        camera.position.y += fabsf(you.stride) * 0.035f * bob;
        camera.target = Vector3Add(camera.position, lookDirection(you.yaw, you.pitch + match.recoil));
    } else {
        // Dead: look down at your body, toward whoever got you
        float t = fminf((match.clock - you.diedAt) / RESPAWN_DELAY, 1);
        camera.position = { you.position.x, you.position.y + 2.2f + t * 1.5f, you.position.z };
        Vector3 target = you.position;
        if (you.killedBy >= 0) target = Vector3Lerp(you.position, match.soldiers[you.killedBy].position, 0.35f);
        camera.target = target;
        Vector3 back = Vector3Subtract(camera.position, target);
        back.y = 0;
        if (Vector3Length(back) < 0.1f) back = { 0, 0, 1 };
        camera.position = Vector3Add(camera.position, Vector3Scale(Vector3Normalize(back), 2.5f));
        camera.fovy = 60;
    }
    return camera;
}

static SoldierStyle teamStyle(const Soldier& s) {
    SoldierStyle style;
    style.aimingForward = true;
    style.stride = s.stride;
    if (s.team == 0) {
        style.visor = { 90, 170, 240, 255 };
        style.patch = { 70, 140, 230, 255 };
    } else {
        style.uniform = { 64, 58, 54, 255 };
        style.vest = { 96, 76, 62, 255 };
        style.visor = { 235, 75, 55, 255 };
        style.patch = { 215, 60, 50, 255 };
    }
    return style;
}

// Your gun, drawn small and close to the camera so it never pokes into walls
static void drawViewModel(App& app, const Camera3D& camera) {
    const Match& match = app.match;
    const Soldier& you = match.soldiers[0];
    if (!you.alive) return;
    const Gun& gun = you.gun();
    float aim = match.aimAmount;
    if (isClass(gun, "Sniper Rifle") && aim > 0.85f) return;   // looking through the scope instead

    rlPushMatrix();
    rlTranslatef(camera.position.x, camera.position.y, camera.position.z);
    rlRotatef(you.yaw, 0, 1, 0);
    rlRotatef(-(you.pitch + match.recoil), 1, 0, 0);
    // Now +z is straight ahead, -x is to the right, +y is up

    const float scale = 0.15f;
    Vector3 hip = { -0.056f, -0.058f, 0.155f };
    Vector3 aimed = { 0, -0.04f, 0.13f };
    Vector3 p = Vector3Lerp(hip, aimed, aim);

    // Sway and bob
    float still = 1 - aim * 0.85f;
    p.x += sinf(match.swayTime * 1.3f) * 0.002f * still + sinf(you.stridePhase) * 0.005f * fabsf(you.stride) * still;
    p.y += cosf(match.swayTime * 1.7f) * 0.0015f * still - fabsf(you.stride) * 0.004f * still;
    p.z -= match.recoil * 0.004f;   // kicks back when firing

    rlTranslatef(p.x, p.y, p.z);

    // Lower the gun while reloading or sprinting
    float lower = 0;
    if (gun.reloadTimer > 0) {
        float t = 1 - gun.reloadTimer / reloadTime(gun);   // 0 to 1 through the reload
        lower = sinf(t * PI);
    }
    bool sprinting = match.sprinting && you.onGround;
    if (sprinting) rlRotatef(25, 0, 1, 0);
    rlRotatef((lower * 30 + (sprinting ? 15 : 0)), 1, 0, 0);
    rlTranslatef(0, -lower * 0.03f, 0);

    app.assets.drawWeapon(gun.build, { 0, 0, 0 }, -90, scale);

    // Muzzle flash
    if (match.clock - you.lastShot < 0.04f) {
        float length = WEAPONS[gun.build.weapon].bodyLength * scale;
        DrawCube({ 0, 0.004f, length * 0.95f }, 0.02f, 0.02f, 0.03f, { 255, 220, 140, 255 });
    }
    rlPopMatrix();
}

static void drawWorld(App& app, const Camera3D& camera) {
    const Match& match = app.match;

    // Sky: a gradient behind everything
    ui::verticalGradient({ 0, 0, (float)ui::WIDTH, (float)ui::HEIGHT }, { 96, 116, 142, 255 }, { 196, 172, 146, 255 });

    // Near and far limits for this 3D view: things closer than 2 cm or further than 300 m aren't drawn
    rlSetClipPlanes(0.02, 300);
    ui::begin3D(camera);

    match.map.draw(camera.position);

    for (const Soldier& s : match.soldiers) {
        if (s.isPlayer) continue;
        SoldierStyle style = teamStyle(s);
        if (s.alive) {
            app.assets.drawSoldier(s.gun().build, s.position, s.yaw, match.clock, style);
            if (match.clock - s.lastShot < 0.05f) {
                DrawCube(muzzlePosition(match, s), 0.12f, 0.12f, 0.12f, { 255, 220, 140, 255 });
            }
        } else {
            // Fall over backwards, then sink into the floor
            float t = match.clock - s.diedAt;
            if (t > 2.6f) continue;
            float fall = fminf(t / 0.45f, 1);
            float sink = t > 2 ? (t - 2) * 0.8f : 0;
            style.stride = 0;
            rlPushMatrix();
            rlTranslatef(s.position.x, s.position.y - sink, s.position.z);
            rlRotatef(s.yaw, 0, 1, 0);
            rlRotatef(-fall * 85, 1, 0, 0);
            app.assets.drawSoldier(s.gun().build, { 0, 0, 0 }, 0, 0, style);
            rlPopMatrix();
        }
    }

    for (const Tracer& t : match.tracers) {
        DrawLine3D(t.from, t.to, Fade({ 255, 230, 160, 255 }, Clamp(t.life / 0.06f, 0, 1) * 0.8f));
    }
    for (const Puff& p : match.puffs) {
        DrawCube(p.position, p.size, p.size, p.size, Fade(p.color, Clamp(p.life / 0.35f, 0, 1)));
    }

    drawViewModel(app, camera);

    ui::end3D();
    rlSetClipPlanes(0.01, 1000);   // back to raylib's normal limits for the menus
}

// ------------------------------------------------------------
// The screen
// ------------------------------------------------------------

static void resume(Match& match) {
    match.paused = false;
    DisableCursor();
    match.mouseSkipFrames = 3;
}

void updateMatch(App& app) {
    Match& match = app.match;
    float dt = fminf(GetFrameTime(), 0.05f);   // a long freeze shouldn't teleport anyone

    // Pause with ESC, or when you click out of the window
    bool justPaused = false;
    if (!match.paused && match.phase != MatchPhase::Ended &&
        (IsKeyPressed(KEY_ESCAPE) || !IsWindowFocused())) {
        match.paused = true;
        justPaused = true;
        EnableCursor();
    } else if (match.paused && IsKeyPressed(KEY_ESCAPE)) {
        resume(match);
    }

    match.showScoreboard = IsKeyDown(KEY_TAB);
    if (!match.paused) simulate(app, dt);
    if (app.screen != Screen::Match) return;   // the match just finished

    Camera3D camera = matchCamera(match, app.settings);
    drawWorld(app, camera);
    if (!match.paused) drawMatchHud(app);

    if (match.paused && !justPaused) {
        if (drawPauseMenu(app)) {
            // Leaving early: you keep the XP from your kills, but no match bonus
            for (int g = 0; g < 2; g++) {
                const Gun& gun = match.soldiers[0].guns[g];
                if (match.gunXp[g] > 0) addWeaponXp(app.profile.weaponProgress[gun.build.weapon], match.gunXp[g]);
            }
            int total = match.gunXp[0] + match.gunXp[1];
            if (total > 0) ui::showToast(TextFormat("Left the match: +%d weapon XP", total));
            app.screen = Screen::Lobby;
        } else if (!match.paused) {
            resume(match);   // RESUME was clicked
        }
    }
}

// The match is over: hand out weapon XP and show the results screen
void finishMatch(App& app) {
    Match& match = app.match;
    const Soldier& you = match.soldiers[0];

    // Finishing a match gives a bonus (more for a win) to the gun you got the most kills with
    int favorite = (match.gunKills[1] > match.gunKills[0]) ? 1 : 0;
    match.gunXp[favorite] += (match.winner == 0) ? 600 : 300;

    for (int g = 0; g < 2; g++) {
        WeaponXpResult& r = match.results[g];
        r.weapon = you.guns[g].build.weapon;
        r.xp = match.gunXp[g];
        WeaponProgress& progress = app.profile.weaponProgress[r.weapon];
        r.levelBefore = progress.level;
        r.xpBefore = progress.xp;
        addWeaponXp(progress, r.xp);
        r.levelAfter = progress.level;
        r.xpAfter = progress.xp;

        // What did the new levels unlock?
        const WeaponDef& weapon = WEAPONS[r.weapon];
        for (const Attachment& a : weapon.attachments) {
            if (a.unlockLevel > r.levelBefore && a.unlockLevel <= r.levelAfter) r.unlocks.push_back(a.name);
        }
        for (const Camo& c : CAMOS) {
            if (c.unlockLevel > r.levelBefore && c.unlockLevel <= r.levelAfter) r.unlocks.push_back(c.name + " camo");
        }
    }
    match.resultsReady = true;
    EnableCursor();
    app.screen = Screen::MatchResults;
}
