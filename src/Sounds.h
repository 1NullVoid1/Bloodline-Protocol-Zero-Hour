#pragma once

// ------------------------------------------------------------
// Sound effects. They're made by code when the game starts
// (mixing noise and tones), so there are no sound files to load.
// If the PC has no speakers or the audio fails to start, every
// function here quietly does nothing.
// ------------------------------------------------------------
namespace sfx {

void load();     // call once after InitAudioDevice()
void unload();

// pitch: 1 = normal, lower sounds like a bigger gun.
// volume: 0 to 1. pan: 0 = right ear, 0.5 = both, 1 = left ear.
void gunshot(float pitch, float volume, float pan);
void hitMarker();
void killConfirm();
void hurt();
void reloadStart();
void reloadEnd();
void dryFire();

}
