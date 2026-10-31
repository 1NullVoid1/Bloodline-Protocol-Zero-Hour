#include "Sounds.h"
#include "raylib.h"
#include <cmath>
#include <vector>

namespace sfx {

static const int SAMPLE_RATE = 44100;   // samples per second
static const float TAU = 6.2831853f;    // one full circle in radians (2 * PI)

static bool loaded = false;

// Several copies of the gunshot, so shots can overlap instead of cutting each other off
static const int SHOT_COPIES = 12;
static Sound shot;
static Sound shotCopies[SHOT_COPIES];
static int nextShot = 0;

static Sound hit, kill, ouch, clickIn, clickOut, click;

// A random number from -1 to 1 (random numbers sound like hiss, or a "shhh")
static float noise() {
    return GetRandomValue(-10000, 10000) / 10000.0f;
}

// Turn a list of samples (-1 to 1) into a sound raylib can play
static Sound soundFromSamples(const std::vector<float>& samples) {
    Wave wave = {};
    wave.frameCount = (unsigned int)samples.size();
    wave.sampleRate = SAMPLE_RATE;
    wave.sampleSize = 16;    // 16-bit numbers, like a CD
    wave.channels = 1;       // mono
    short* data = (short*)MemAlloc((unsigned int)(samples.size() * sizeof(short)));
    for (size_t i = 0; i < samples.size(); i++) {
        float v = samples[i];
        if (v > 1) v = 1;
        if (v < -1) v = -1;
        data[i] = (short)(v * 32000);
    }
    wave.data = data;
    Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave);        // the sound made its own copy
    return sound;
}

static std::vector<float> makeGunshot() {
    std::vector<float> s((size_t)(SAMPLE_RATE * 0.45f));
    float smooth = 0;
    for (size_t i = 0; i < s.size(); i++) {
        float t = (float)i / SAMPLE_RATE;
        float n = noise();
        smooth += (n - smooth) * 0.2f;                      // smoothed noise is deeper, less hissy
        float crack = n * expf(-t * 70);                    // sharp crack at the start
        float boom = smooth * 2.2f * expf(-t * 9);          // the rumble that fades out
        float thump = sinf(TAU * 65 * t) * expf(-t * 22);   // a low punch you feel
        s[i] = 0.45f * crack + 0.8f * boom + 0.7f * thump;
    }
    return s;
}

static std::vector<float> makeTone(float seconds, float frequency, float fade, float noiseAmount) {
    std::vector<float> s((size_t)(SAMPLE_RATE * seconds));
    for (size_t i = 0; i < s.size(); i++) {
        float t = (float)i / SAMPLE_RATE;
        s[i] = (sinf(TAU * frequency * t) + noise() * noiseAmount) * expf(-t * fade) * 0.6f;
    }
    return s;
}

static std::vector<float> makeKill() {
    // Two quick rising tones: "ding-ding"
    std::vector<float> s((size_t)(SAMPLE_RATE * 0.3f));
    for (size_t i = 0; i < s.size(); i++) {
        float t = (float)i / SAMPLE_RATE;
        float frequency = (t < 0.07f) ? 1100.0f : 1650.0f;
        float start = (t < 0.07f) ? 0 : 0.07f;
        s[i] = sinf(TAU * frequency * t) * expf(-(t - start) * 16) * 0.45f;
    }
    return s;
}

static std::vector<float> makeOuch() {
    std::vector<float> s((size_t)(SAMPLE_RATE * 0.25f));
    float smooth = 0;
    for (size_t i = 0; i < s.size(); i++) {
        float t = (float)i / SAMPLE_RATE;
        smooth += (noise() - smooth) * 0.08f;
        s[i] = (sinf(TAU * 85 * t) * 0.8f + smooth * 2) * expf(-t * 16);
    }
    return s;
}

void load() {
    if (!IsAudioDeviceReady()) {
        TraceLog(LOG_WARNING, "SOUNDS: no audio device, playing without sound");
        return;
    }
    shot = soundFromSamples(makeGunshot());
    for (int i = 0; i < SHOT_COPIES; i++) shotCopies[i] = LoadSoundAlias(shot);
    hit = soundFromSamples(makeTone(0.07f, 2600, 60, 0.3f));
    kill = soundFromSamples(makeKill());
    ouch = soundFromSamples(makeOuch());
    clickIn = soundFromSamples(makeTone(0.05f, 1900, 120, 0.8f));
    clickOut = soundFromSamples(makeTone(0.06f, 2900, 110, 0.8f));
    click = soundFromSamples(makeTone(0.03f, 3500, 200, 0.5f));
    loaded = true;
}

void unload() {
    if (!loaded) return;
    for (int i = 0; i < SHOT_COPIES; i++) UnloadSoundAlias(shotCopies[i]);
    UnloadSound(shot);
    UnloadSound(hit);
    UnloadSound(kill);
    UnloadSound(ouch);
    UnloadSound(clickIn);
    UnloadSound(clickOut);
    UnloadSound(click);
    loaded = false;
}

void gunshot(float pitch, float volume, float pan) {
    if (!loaded) return;
    Sound& s = shotCopies[nextShot];
    nextShot = (nextShot + 1) % SHOT_COPIES;
    SetSoundPitch(s, pitch);
    SetSoundVolume(s, volume);
    SetSoundPan(s, pan);
    PlaySound(s);
}

void hitMarker()   { if (loaded) PlaySound(hit); }
void killConfirm() { if (loaded) PlaySound(kill); }
void hurt()        { if (loaded) PlaySound(ouch); }
void reloadStart() { if (loaded) PlaySound(clickIn); }
void reloadEnd()   { if (loaded) PlaySound(clickOut); }
void dryFire()     { if (loaded) PlaySound(click); }

}
