#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "global.h"
#include "functions.h"
#include "variables.h"
#include "macros.h"

extern GameState* gGameState;
extern SaveContext gSaveContext;

void Sram_InitDebugSave(void);
void Select_LoadGame(SelectContext* selectContext, s32 entranceIndex);
}

namespace {

constexpr const char* kLaunchIntentFileName = "projectzelda64_launch_intent.json";
constexpr s32 kHappyMaskShopEntrance = 0x0530; // ENTR_HAPPY_MASK_SHOP_0

bool gProjectZelda64IntentConsumed = false;

std::string ReadWholeFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return {};
    }

    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool Contains(const std::string& text, const char* value) {
    return text.find(value) != std::string::npos;
}

bool ConsumeLaunchIntentFromPath(const std::filesystem::path& path) {
    std::error_code existsError;
    if (!std::filesystem::exists(path, existsError) || existsError) {
        return false;
    }

    const std::string intent = ReadWholeFile(path);

    const bool isOot =
        Contains(intent, "\"targetGame\": \"oot\"") ||
        Contains(intent, "\"targetGame\":\"oot\"");

    const bool isHappyMaskTarget =
        Contains(intent, "Happy Mask") ||
        Contains(intent, "HAPPY_MASK") ||
        Contains(intent, "ENTR_HAPPY_MASK_SHOP_0") ||
        Contains(intent, "ENTR_MARKET_DAY_OUTSIDE_HAPPY_MASK_SHOP") ||
        Contains(intent, "happy_mask_shop");

    if (!isOot || !isHappyMaskTarget) {
        return false;
    }

    std::error_code removeError;
    std::filesystem::remove(path, removeError);

    std::cout << "[ProjectZelda64] consumed OoT launch intent: " << path.string() << '\n';
    return true;
}

bool TryConsumeHappyMaskLaunchIntent() {
    if (gProjectZelda64IntentConsumed) {
        return false;
    }

    const auto currentPath = std::filesystem::current_path();

    const bool consumed =
        ConsumeLaunchIntentFromPath(currentPath / kLaunchIntentFileName) ||
        ConsumeLaunchIntentFromPath(currentPath / "x64" / "Release" / kLaunchIntentFileName) ||
        ConsumeLaunchIntentFromPath(currentPath / "build" / "x64" / "Release" / kLaunchIntentFileName) ||
        ConsumeLaunchIntentFromPath(currentPath / "extern" / "Shipwright" / "x64" / "Release" / kLaunchIntentFileName) ||
        ConsumeLaunchIntentFromPath(currentPath / "extern" / "Shipwright" / "build" / "x64" / "Release" / kLaunchIntentFileName);

    if (consumed) {
        gProjectZelda64IntentConsumed = true;
    }

    return consumed;
}

void PrepareHappyMaskShopSaveState() {
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.fileNum = 0xFE;
    Sram_InitDebugSave();
    gSaveContext.fileNum = 0xFF;

    gSaveContext.entranceIndex = kHappyMaskShopEntrance;
    gSaveContext.sceneSetupIndex = 0;
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.linkAge = LINK_AGE_CHILD;
    gSaveContext.nightFlag = 0;
    gSaveContext.skyboxTime = gSaveContext.dayTime = 0x8000;
    gSaveContext.seqId = static_cast<u8>(NA_BGM_DISABLED);
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.showTitleCard = true;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK_FAST;

    for (int buttonIndex = 0; buttonIndex < ARRAY_COUNT(gSaveContext.buttonStatus); buttonIndex++) {
        gSaveContext.buttonStatus[buttonIndex] = BTN_ENABLED;
    }

    gWeatherMode = 0;
}

void BootDirectlyToHappyMaskShopIfIntentExists(void* gameState) {
    if (!TryConsumeHappyMaskLaunchIntent()) {
        return;
    }

    PrepareHappyMaskShopSaveState();

    auto* titleContext = static_cast<TitleContext*>(gameState);
    titleContext->exit = true;
    titleContext->state.running = false;

    STOP_GAMESTATE(&titleContext->state);
    Select_LoadGame(reinterpret_cast<SelectContext*>(&titleContext->state), kHappyMaskShopEntrance);

    std::cout << "[ProjectZelda64] booting OoT directly to Happy Mask Shop\n";
}

void ApplyHappyMaskLaunchIntentOnLoadGame(int32_t) {
    if (TryConsumeHappyMaskLaunchIntent()) {
        PrepareHappyMaskShopSaveState();
        gSaveContext.entranceIndex = kHappyMaskShopEntrance;
        std::cout << "[ProjectZelda64] applied OoT launch intent on load game\n";
    }
}

void RegisterProjectZelda64LaunchIntent() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnZTitleInit>(BootDirectlyToHappyMaskShopIfIntentExists);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>(ApplyHappyMaskLaunchIntentOnLoadGame);
}

static RegisterShipInitFunc initFunc(RegisterProjectZelda64LaunchIntent);

} // namespace
