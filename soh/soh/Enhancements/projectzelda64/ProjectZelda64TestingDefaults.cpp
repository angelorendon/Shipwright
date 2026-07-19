// ProjectZelda64 testing defaults.
// Keep this file limited to branch-stable CVars. Do not use input hooks here:
// this Shipwright branch does not expose OnPassPlayerInputs.

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/ShipInit.hpp"

namespace {
constexpr const char* kEnableTestingDefaultsCVar = "gProjectZelda64.EnableTestingDefaults";

void ApplyProjectZelda64TestingDefaults() {
    // Graphics defaults for repeated ProjectZelda64 smoke tests.
    CVarSetInteger("gSettings.MSAAValue", 8);
    CVarSetInteger("gInterpolationFPS", 300);
    CVarSetInteger("gSdlWindowedFullscreen", 1);

    // Speed Modifier defaults for quickly reaching the Happy Mask Shop during OoT portal tests.
    // Mode 3 is toggle mode in the Shipwright/2S2H speed modifier convention.
    CVarSetInteger("gCheats.SpeedModifier.Enabled", 1);
    CVarSetInteger("gCheats.SpeedModifier.Mode", 3);
    CVarSetFloat("gCheats.SpeedModifier.Value", 5.0f);

    // Shipwright forks/versions have used slightly different names for the jump-distance/velocity guard.
    // Setting these harmless extra keys keeps Angelo's integration fork resilient across local rebases.
    CVarSetInteger("gCheats.SpeedModifier.DontAffectJumpVelocity", 1);
    CVarSetInteger("gCheats.SpeedModifier.DoNotAffectJumpVelocity", 1);
    CVarSetInteger("gCheats.SpeedModifier.DontAffectJumpDistance", 1);
    CVarSetInteger("gCheats.SpeedModifier.DoNotAffectJumpDistance", 1);

    // Skips and speed-ups.
    CVarSetInteger("gEnhancements.TimeSavers.SkipCutscene.Intro", 1);
    CVarSetInteger("gEnhancements.TimeSavers.SkipCutscene.Entrances", 1);
    CVarSetInteger("gEnhancements.TimeSavers.SkipCutscene.Story", 1);
    CVarSetInteger("gEnhancements.TimeSavers.SkipCutscene.LearnSong", 1);
    CVarSetInteger("gEnhancements.TimeSavers.SkipCutscene.BossIntro", 1);
    CVarSetInteger("gEnhancements.TimeSavers.SkipCutscene.QuickBossDeaths", 1);
    CVarSetInteger("gEnhancements.TimeSavers.SkipCutscene.OnePoint", 1);
    CVarSetInteger("gEnhancements.TimeSavers.SkipOwlInteractions", 1);
    CVarSetInteger("gEnhancements.TextSpeed", 5);
    CVarSetInteger("gEnhancements.SlowTextSpeed", 5);
}

void RegisterProjectZelda64TestingDefaults() {
    if (CVarGetInteger(kEnableTestingDefaultsCVar, 1)) {
        ApplyProjectZelda64TestingDefaults();
    }
}
} // namespace

static RegisterShipInitFunc initFunc(RegisterProjectZelda64TestingDefaults, { kEnableTestingDefaultsCVar });
