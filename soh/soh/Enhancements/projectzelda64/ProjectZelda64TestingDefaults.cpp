#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "variables.h"
}

namespace {
constexpr const char* kEnableTestingDefaultsCVar = "gProjectZelda64.EnableTestingDefaults";
constexpr const char* kSwapAAndBCVar = "gProjectZelda64.SwapAAndB";

void ApplyProjectZelda64TestingDefaults() {
    // Graphics defaults for repeated ProjectZelda64 smoke tests.
    CVarSetInteger("gSettings.MSAAValue", 8);
    CVarSetInteger("gInterpolationFPS", 360);

    // Speed Modifier defaults for quickly reaching the Happy Mask Shop during OoT portal tests.
    // Mode 3 is toggle mode in the Shipwright/2S2H speed modifier convention.
    CVarSetInteger("gCheats.SpeedModifier.Mode", 3);
    CVarSetFloat("gCheats.SpeedModifier.Value", 5.0f);
    CVarSetInteger("gCheats.SpeedModifier.Btn", BTN_CUSTOM_MODIFIER1);

    // Shipwright forks/versions have used slightly different names for the jump-distance/velocity guard.
    // Setting these harmless extra keys keeps Angelo's integration fork resilient across local rebases.
    CVarSetInteger("gCheats.SpeedModifier.DontAffectJumpVelocity", 1);
    CVarSetInteger("gCheats.SpeedModifier.DoNotAffectJumpVelocity", 1);
    CVarSetInteger("gCheats.SpeedModifier.DontAffectJumpDistance", 1);
    CVarSetInteger("gCheats.SpeedModifier.DoNotAffectJumpDistance", 1);

    // Skips and speed-ups.
    CVarSetInteger("gEnhancements.TimeSavers.SkipCutscene.Intro", 1);
    CVarSetInteger("gEnhancements.TimeSavers.SkipCutscene.Entrances", 1);
    CVarSetInteger("gEnhancements.TimeSavers.SkipOwlInteractions", 1);
    CVarSetInteger("gEnhancements.TextSpeed", 5);
    CVarSetInteger("gEnhancements.SlowTextSpeed", 5);

    // Keep the input swap discoverable/disableable without depending on controller profile serialization.
    CVarSetInteger(kSwapAAndBCVar, 1);
}

void SwapAAndBButtons(Input* input) {
    if (input == nullptr) {
        return;
    }

    auto swapButtons = [](decltype(input->cur.button)& buttons) {
        const bool hasA = (buttons & BTN_A) != 0;
        const bool hasB = (buttons & BTN_B) != 0;

        buttons &= static_cast<decltype(buttons)>(~(BTN_A | BTN_B));
        if (hasA) {
            buttons |= BTN_B;
        }
        if (hasB) {
            buttons |= BTN_A;
        }
    };

    swapButtons(input->cur.button);
    swapButtons(input->press.button);
    swapButtons(input->rel.button);
}

void RegisterProjectZelda64TestingDefaults() {
    if (CVarGetInteger(kEnableTestingDefaultsCVar, 1)) {
        ApplyProjectZelda64TestingDefaults();
    }

    COND_HOOK(OnPassPlayerInputs, CVarGetInteger(kSwapAAndBCVar, 1), [](Input* input) { SwapAAndBButtons(input); });
}
} // namespace

static RegisterShipInitFunc initFunc(RegisterProjectZelda64TestingDefaults,
                                     { kEnableTestingDefaultsCVar, kSwapAAndBCVar });
