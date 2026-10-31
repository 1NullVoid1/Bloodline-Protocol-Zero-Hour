#include "Match.h"
#include "raymath.h"
#include <cmath>

// ------------------------------------------------------------
// The bots' brains.
// Each bot is always doing one of two things:
//   FIGHTING: it can see an enemy. It turns to aim at them, waits a moment
//             to "react", then shoots in bursts while strafing.
//   ROAMING:  it walks a path across the map, toward where it heard shots,
//             where it got hit from, or where an enemy might be.
// Change the numbers below to make bots easier or harder.
// ------------------------------------------------------------

static const float SIGHT_RANGE = 50;       // meters they can spot you from
static const float VIEW_ANGLE = 60;        // degrees to each side of where they look
static const float REACTION_MIN = 0.5f;    // seconds before shooting at someone they just spotted
static const float REACTION_MAX = 0.9f;
static const float TURN_SPEED = 300;       // degrees per second
static const float AIM_ERROR = 7;          // degrees their aim is off when they first spot you...
static const float AIM_SETTLE = 0.9f;      // ...and how fast that gets better while they track you
static const float SPREAD = 1.0f;          // their bullet spread, compared to your hip fire
static const float HEARING_RANGE = 25;     // meters they hear gunshots from

static float randomFloat(float min, float max) {
    return min + (max - min) * GetRandomValue(0, 10000) / 10000.0f;
}

// The difference between two angles, from -180 to 180
static float angleDifference(float from, float to) {
    float d = fmodf(to - from + 540, 360) - 180;
    return d;
}

// Turn an angle toward a target angle, but no faster than maxStep
static float turnToward(float current, float target, float maxStep) {
    float d = angleDifference(current, target);
    if (d > maxStep) d = maxStep;
    if (d < -maxStep) d = -maxStep;
    return current + d;
}

static Vector3 chestOf(const Soldier& s) {
    return { s.position.x, s.position.y + (s.crouching ? 0.85f : 1.2f), s.position.z };
}

void setupBots(Match& match) {
    for (Soldier& s : match.soldiers) {
        s.brain.thinkTimer = randomFloat(0, 0.3f);   // so they don't all think on the same frame
    }
}

// Who can this bot see? Returns the closest visible enemy, or -1.
static int findVisibleEnemy(const Match& match, int index) {
    const Soldier& bot = match.soldiers[index];
    Vector3 eye = bot.eye();
    Vector3 facing = { sinf(bot.yaw * DEG2RAD), 0, cosf(bot.yaw * DEG2RAD) };
    int best = -1;
    float bestDistance = SIGHT_RANGE;
    for (int i = 0; i < (int)match.soldiers.size(); i++) {
        const Soldier& enemy = match.soldiers[i];
        if (!enemy.alive || enemy.team == bot.team) continue;
        Vector3 to = Vector3Subtract(chestOf(enemy), eye);
        float distance = Vector3Length(to);
        if (distance > bestDistance) continue;

        // Is the enemy in front of them? (Very close enemies get noticed anyway.)
        Vector3 flat = Vector3Normalize({ to.x, 0, to.z });
        float angle = acosf(Clamp(Vector3DotProduct(flat, facing), -1, 1)) * RAD2DEG;
        if (angle > VIEW_ANGLE && distance > 5) continue;

        // Is there a wall in the way? Check the chest and the head.
        if (!match.map.canSee(eye, chestOf(enemy)) && !match.map.canSee(eye, enemy.eye())) continue;

        best = i;
        bestDistance = distance;
    }
    return best;
}

// Choose somewhere to walk to and work out the path
static void pickGoal(Match& match, int index) {
    Soldier& bot = match.soldiers[index];
    BotBrain& brain = bot.brain;
    const Map& map = match.map;
    if (map.waypoints.empty()) return;

    int start = map.nearestWaypoint(bot.position);
    int goal;
    if (brain.hasCheckSpot) {
        goal = map.nearestWaypoint(brain.checkSpot);
    } else if (GetRandomValue(0, 99) < 25) {
        // Hunt: head roughly toward a random enemy
        std::vector<int> enemies;
        for (int i = 0; i < (int)match.soldiers.size(); i++) {
            if (match.soldiers[i].alive && match.soldiers[i].team != bot.team) enemies.push_back(i);
        }
        if (enemies.empty()) {
            goal = GetRandomValue(0, (int)map.waypoints.size() - 1);
        } else {
            Vector3 p = match.soldiers[enemies[GetRandomValue(0, (int)enemies.size() - 1)]].position;
            p.x += randomFloat(-8, 8);
            p.z += randomFloat(-8, 8);
            goal = map.nearestWaypoint(p);
        }
    } else {
        goal = GetRandomValue(0, (int)map.waypoints.size() - 1);   // wander anywhere
    }

    brain.path = map.findPath(start, goal);
    brain.pathStep = 0;
    brain.hasGoal = !brain.path.empty();
}

// Walk along the path. Returns the direction to walk in.
static Vector3 followPath(Match& match, int index) {
    Soldier& bot = match.soldiers[index];
    BotBrain& brain = bot.brain;
    if (!brain.hasGoal || brain.pathStep >= (int)brain.path.size()) {
        if (brain.hasGoal) brain.hasCheckSpot = false;   // got there, nothing to see
        pickGoal(match, index);
        if (!brain.hasGoal) return { 0, 0, 0 };
    }
    Vector3 next = match.map.waypoints[brain.path[brain.pathStep]].position;
    Vector3 to = { next.x - bot.position.x, 0, next.z - bot.position.z };
    if (Vector3Length(to) < 0.8f) {
        brain.pathStep++;
        return Vector3Normalize(to);
    }
    return Vector3Normalize(to);
}

static float preferredDistance(const Gun& gun) {
    const std::string& c = WEAPONS[gun.build.weapon].weaponClass;
    if (c == "Shotgun") return 6;
    if (c == "SMG" || c == "Pistol") return 13;
    if (c == "Sniper Rifle") return 40;
    return 22;
}

void updateBot(Match& match, int index, float dt) {
    Soldier& bot = match.soldiers[index];
    if (!bot.alive) return;
    BotBrain& brain = bot.brain;
    Gun& gun = bot.gun();

    // Forget a target that died
    if (brain.target >= 0 && !match.soldiers[brain.target].alive) brain.target = -1;

    // ---- Look around a few times a second (looking every frame would be slow) ----
    brain.thinkTimer -= dt;
    if (brain.thinkTimer <= 0) {
        brain.thinkTimer = randomFloat(0.15f, 0.25f);
        int seen = findVisibleEnemy(match, index);
        if (seen >= 0) {
            if (seen != brain.target) {
                // Spotted someone new: it takes a moment to react, and the first shots are off
                brain.target = seen;
                float distance = Vector3Distance(bot.position, match.soldiers[seen].position);
                brain.reaction = randomFloat(REACTION_MIN, REACTION_MAX) + (distance > 30 ? 0.2f : 0);
                brain.aimErrorYaw = randomFloat(-AIM_ERROR, AIM_ERROR);
                brain.aimErrorPitch = randomFloat(-AIM_ERROR, AIM_ERROR) * 0.5f;
                brain.burstLeft = 0;
            }
            brain.lastSawTarget = match.clock;
            brain.lastSeenPosition = match.soldiers[seen].position;
        } else if (brain.target >= 0 && match.clock - brain.lastSawTarget > 1.0f) {
            // Lost them: go and check where they were last seen
            brain.target = -1;
            brain.checkSpot = brain.lastSeenPosition;
            brain.hasCheckSpot = true;
            brain.hasGoal = false;
        }

        // Smooth out the path: skip waypoints we can already walk straight to
        if (brain.hasGoal) {
            int skips = 0;
            while (brain.pathStep + 1 < (int)brain.path.size() && skips < 3 &&
                   match.map.canWalk(bot.position, match.map.waypoints[brain.path[brain.pathStep + 1]].position)) {
                brain.pathStep++;
                skips++;
            }
        }
    }

    Vector3 wish = { 0, 0, 0 };
    float speed = moveSpeed(gun);
    bool fighting = brain.target >= 0 && match.clock - brain.lastSawTarget < 0.4f;

    if (fighting) {
        // ---- FIGHTING ----
        const Soldier& target = match.soldiers[brain.target];
        Vector3 to = Vector3Subtract(chestOf(target), bot.eye());
        float flatDistance = sqrtf(to.x * to.x + to.z * to.z);
        float wantYaw = atan2f(to.x, to.z) * RAD2DEG;
        float wantPitch = atan2f(to.y, flatDistance) * RAD2DEG;

        // Aim gets better the longer they track you
        float settle = expf(-AIM_SETTLE * dt);
        brain.aimErrorYaw *= settle;
        brain.aimErrorPitch *= settle;
        bot.yaw = turnToward(bot.yaw, wantYaw + brain.aimErrorYaw, TURN_SPEED * dt);
        bot.pitch = turnToward(bot.pitch, wantPitch + brain.aimErrorPitch, TURN_SPEED * dt);

        // Shoot once they've reacted and are roughly on target
        brain.reaction -= dt;
        brain.burstPause -= dt;
        float offTarget = fabsf(angleDifference(bot.yaw, wantYaw)) + fabsf(bot.pitch - wantPitch);
        bool canSeeNow = match.clock - brain.lastSawTarget < 0.3f;
        if (canSeeNow && brain.reaction <= 0 && offTarget < 6 && brain.burstPause <= 0 &&
            gun.cooldown <= 0 && gun.reloadTimer <= 0 && gun.ammo > 0) {
            fireGun(match, index, hipSpread(gun) * SPREAD);
            if (isAutomatic(gun)) {
                // Automatic guns: bursts of a few shots with short pauses
                if (brain.burstLeft <= 0) brain.burstLeft = GetRandomValue(3, 7);
                brain.burstLeft--;
                if (brain.burstLeft <= 0) brain.burstPause = randomFloat(0.4f, 0.8f);
            } else {
                // One click at a time, with a little hesitation
                gun.cooldown += randomFloat(0.2f, 0.5f);
                if (WEAPONS[gun.build.weapon].weaponClass == "Sniper Rifle") gun.cooldown += 0.4f;
            }
        }

        // Strafe side to side, and get to a good distance for their gun
        brain.strafeTimer -= dt;
        if (brain.strafeTimer <= 0) {
            brain.strafe = GetRandomValue(0, 1) ? 1.0f : -1.0f;
            brain.strafeTimer = randomFloat(0.5f, 1.3f);
        }
        Vector3 toTarget = flatDistance > 0.01f ? Vector3{ to.x / flatDistance, 0, to.z / flatDistance } : Vector3{ 0, 0, 1 };
        Vector3 right = { -toTarget.z, 0, toTarget.x };
        wish = Vector3Scale(right, brain.strafe);
        float preferred = preferredDistance(gun);
        if (flatDistance > preferred * 1.3f) wish = Vector3Add(wish, toTarget);
        if (flatDistance < preferred * 0.5f) wish = Vector3Subtract(wish, Vector3Scale(toTarget, 0.7f));
        if (Vector3Length(wish) > 0) wish = Vector3Normalize(wish);
        speed *= 0.75f;
    } else {
        // ---- ROAMING ----
        wish = followPath(match, index);
        speed *= 1.15f;   // they jog between fights
        if (Vector3Length(wish) > 0.1f) {
            bot.yaw = turnToward(bot.yaw, atan2f(wish.x, wish.z) * RAD2DEG, 360 * dt);
        }
        bot.pitch = turnToward(bot.pitch, 0, 90 * dt);

        // Reload between fights
        if (gun.ammo < gun.magazineSize / 2 && gun.reloadTimer <= 0) gun.reloadTimer = reloadTime(gun);

        // Stuck on something? Pick a new place to go.
        brain.stuckTimer += dt;
        if (brain.stuckTimer > 1.2f) {
            if (Vector3Distance(bot.position, brain.stuckCheckPosition) < 0.6f) {
                brain.hasGoal = false;
                brain.hasCheckSpot = false;
            }
            brain.stuckTimer = 0;
            brain.stuckCheckPosition = bot.position;
        }
    }

    if (gun.ammo <= 0 && gun.reloadTimer <= 0) gun.reloadTimer = reloadTime(gun);

    moveSoldier(match, bot, wish, speed, false, dt);
}

// Bots on the other team who hear a gunshot go and check it out
void botHeardShot(Match& match, int shooter) {
    const Soldier& s = match.soldiers[shooter];
    for (Soldier& bot : match.soldiers) {
        if (bot.isPlayer || !bot.alive || bot.team == s.team) continue;
        if (bot.brain.target >= 0 || bot.brain.hasCheckSpot) continue;
        if (Vector3Distance(bot.position, s.position) > HEARING_RANGE) continue;
        bot.brain.checkSpot = s.position;
        bot.brain.hasCheckSpot = true;
        bot.brain.hasGoal = false;
    }
}

// Getting shot: fight back if they can see the shooter, otherwise go find them
void botGotHurt(Match& match, int index, int attacker) {
    Soldier& bot = match.soldiers[index];
    BotBrain& brain = bot.brain;
    const Soldier& a = match.soldiers[attacker];
    if (brain.target == attacker) return;
    if (match.map.canSee(bot.eye(), chestOf(a))) {
        brain.target = attacker;
        brain.lastSawTarget = match.clock;
        brain.lastSeenPosition = a.position;
        brain.reaction = randomFloat(0.15f, 0.3f);
        brain.aimErrorYaw = randomFloat(-AIM_ERROR, AIM_ERROR);
        brain.aimErrorPitch = randomFloat(-AIM_ERROR, AIM_ERROR) * 0.5f;
    } else if (brain.target < 0) {
        brain.checkSpot = a.position;
        brain.hasCheckSpot = true;
        brain.hasGoal = false;
        bot.yaw = atan2f(a.position.x - bot.position.x, a.position.z - bot.position.z) * RAD2DEG;   // spin around
    }
}
