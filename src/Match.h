#pragma once

#include "raylib.h"
#include "GameData.h"
#include "Map.h"
#include "Settings.h"
#include <string>
#include <vector>

// ------------------------------------------------------------
// Team Deathmatch against bots.
// Everyone in the match (you and the bots) is a Soldier.
// soldiers[0] is always you. Team 0 is your team, team 1 the enemy.
// ------------------------------------------------------------

const int TEAM_SIZE = 6;                 // 6v6
const int SCORE_TO_WIN = 75;
const float MATCH_LENGTH = 600;          // seconds (10 minutes)
const float MAX_HEALTH = 100;
const float RESPAWN_DELAY = 3;           // seconds you stay dead
const float COUNTDOWN_LENGTH = 4;        // "MATCH STARTS IN..."
const float END_SCREEN_DELAY = 3;        // how long "VICTORY" shows before the results

const char* const TEAM_NAMES[2] = { "BLOODLINE", "IRON VEIL" };

// A gun in someone's hands: the build from the loadout, plus ammo and timers
struct Gun {
    WeaponBuild build;
    Stats stats;               // stats with attachments (worked out once when the match starts)
    int magazineSize = 0;
    int ammo = 0;              // rounds left in the magazine (spare ammo is unlimited)
    float cooldown = 0;        // seconds until it can fire again
    float reloadTimer = 0;     // more than 0 while reloading
};

// What a bot is thinking. The player doesn't use this.
struct BotBrain {
    int target = -1;               // which soldier it's fighting (-1 = nobody)
    float lastSawTarget = 0;       // match time it last saw its target
    Vector3 lastSeenPosition = { 0, 0, 0 };
    float reaction = 0;            // seconds before it starts shooting at a new target
    float thinkTimer = 0;          // seconds until it looks around for enemies again
    float aimErrorYaw = 0;         // how far off its aim is (shrinks while it tracks you)
    float aimErrorPitch = 0;
    int burstLeft = 0;             // shots left in this burst
    float burstPause = 0;          // waiting between bursts
    float strafe = 1;              // strafing left (-1) or right (+1)
    float strafeTimer = 0;
    std::vector<int> path;         // waypoints to walk through
    int pathStep = 0;
    bool hasGoal = false;
    Vector3 checkSpot = { 0, 0, 0 }; // where it heard shooting or got shot from
    bool hasCheckSpot = false;
    float stuckTimer = 0;
    Vector3 stuckCheckPosition = { 0, 0, 0 };
};

struct Soldier {
    std::string name;
    int team = 0;
    bool isPlayer = false;

    Vector3 position = { 0, 0, 0 };   // at the feet
    Vector3 velocity = { 0, 0, 0 };
    float yaw = 0;                    // looking left/right in degrees (0 = toward +z)
    float pitch = 0;                  // looking up/down in degrees
    bool onGround = true;
    bool crouching = false;
    float eyeHeight = 1.62f;          // goes down smoothly when you crouch
    float stride = 0;                 // leg swing for the walking animation (-1 to 1)
    float stridePhase = 0;

    float health = MAX_HEALTH;
    bool alive = true;
    float diedAt = 0;                 // match time of death (for the falling animation)
    float respawnTimer = 0;
    float lastHurt = -99;             // match time they last got hit (health comes back after a while)
    float lastShot = -99;             // match time they last fired (enemies show on the minimap)
    int killedBy = -1;

    Gun guns[2];                      // 0 = primary, 1 = secondary
    int currentGun = 0;

    int kills = 0;
    int deaths = 0;

    BotBrain brain;

    Gun& gun() { return guns[currentGun]; }
    const Gun& gun() const { return guns[currentGun]; }
    Vector3 eye() const { return { position.x, position.y + eyeHeight, position.z }; }
};

struct KillFeedEntry {
    std::string killer, victim, weapon;
    int killerTeam, victimTeam;
    bool headshot;
    float time;                       // match time it happened
};

// Short-lived effects
struct Tracer { Vector3 from, to; float life; };
struct Puff { Vector3 position; Color color; float life; float size; };

// Little messages under the crosshair, like "+100 KILL"
struct Popup { std::string text; Color color; float life; };

enum class MatchPhase { Countdown, Playing, Ended };

// The results screen needs to know how your weapons leveled up
struct WeaponXpResult {
    int weapon = -1;
    int xp = 0;
    int levelBefore = 1, levelAfter = 1;
    int xpBefore = 0, xpAfter = 0;
    std::vector<std::string> unlocks;
};

struct Match {
    Map map;
    std::vector<Soldier> soldiers;
    int score[2] = { 0, 0 };
    float clock = 0;                  // seconds since the match started (keeps going in every phase)
    float timeLeft = MATCH_LENGTH;
    MatchPhase phase = MatchPhase::Countdown;
    float phaseTimer = COUNTDOWN_LENGTH;
    bool paused = false;
    bool showScoreboard = false;
    int winner = -1;                  // 0 = you won, 1 = they won, -1 = draw

    // Effects and feedback
    std::vector<KillFeedEntry> killFeed;
    std::vector<Tracer> tracers;
    std::vector<Puff> puffs;
    std::vector<Popup> popups;
    float hitMarkerTimer = 0;
    bool hitMarkerKill = false;
    float hurtTimer = 0;              // red flash when you get hit
    Vector3 hurtFrom = { 0, 0, 0 };   // where the shot came from (for the red arrow)
    float aimAmount = 0;              // 0 = hip fire, 1 = fully aiming down sights
    bool sprinting = false;
    float recoil = 0;                 // extra upward kick, recovers over time
    float spreadBloom = 0;            // crosshair opens up while you shoot
    float swayTime = 0;
    int mouseSkipFrames = 0;          // ignore the first mouse moves after locking the cursor

    // Your stats for XP
    int shotsFired = 0;
    int shotsHit = 0;
    int headshots = 0;
    int gunKills[2] = { 0, 0 };       // kills with your primary and secondary
    int gunXp[2] = { 0, 0 };          // XP earned by each of your guns

    // Filled in when the match ends
    WeaponXpResult results[2];
    bool resultsReady = false;
};

struct App;   // App.h includes this file, so we only say "App exists" here

// Match.cpp
void startMatch(App& app);
void updateMatch(App& app);
void finishMatch(App& app);
Camera3D matchCamera(const Match& match, const Settings& settings);

// Gun rules, worked out from the weapon's stats (Match.cpp)
float shotInterval(const Gun& gun);
bool isAutomatic(const Gun& gun);
float reloadTime(const Gun& gun);
float aimTime(const Gun& gun);
float hipSpread(const Gun& gun);
float moveSpeed(const Gun& gun);
float effectiveRange(const Gun& gun);
int pelletCount(const Gun& gun);
float aimZoom(const Gun& gun);
float damageAt(const Gun& gun, float distance, bool headshot);
float playerSpread(const Match& match);   // your bullet spread right now, in degrees
void fireGun(Match& match, int shooter, float spreadDegrees);
void moveSoldier(Match& match, Soldier& s, Vector3 wishDirection, float speed, bool jump, float dt);

// Bots.cpp
void setupBots(Match& match);
void updateBot(Match& match, int index, float dt);
void botHeardShot(Match& match, int shooter);
void botGotHurt(Match& match, int bot, int attacker);

// MatchHud.cpp
void drawMatchHud(App& app);
bool drawPauseMenu(App& app);   // returns true if you chose to leave
void updateMatchResults(App& app);
