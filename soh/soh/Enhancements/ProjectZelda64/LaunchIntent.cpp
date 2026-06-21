#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "global.h"
#include "functions.h"
#include "variables.h"
#include "macros.h"

extern GameState* gGameState;
extern SaveContext gSaveContext;

void Sram_InitDebugSave(void);
}

namespace {

constexpr const char* kLaunchIntentFileName = "projectzelda64_launch_intent.json";
constexpr const char* kSuppressNextPortalFileName = "projectzelda64_suppress_next_oot_portal.txt";
constexpr const char* kOotSaveSnapshotFileName = "projectzelda64_oot_save_snapshot.bin";
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

bool TryRestoreOotSaveSnapshotFromPath(const std::filesystem::path& path) {
    std::error_code existsError;
    if (!std::filesystem::exists(path, existsError) || existsError) {
        return false;
    }

    std::ifstream snapshotFile(path, std::ios::binary);
    if (!snapshotFile.is_open()) {
        return false;
    }

    SaveContext snapshot = {};
    snapshotFile.read(reinterpret_cast<char*>(&snapshot), sizeof(snapshot));

    if (snapshotFile.gcount() != sizeof(snapshot)) {
        std::cout << "[ProjectZelda64] ignored incomplete OoT save snapshot: " << path.string() << '\n';
        return false;
    }

    gSaveContext = snapshot;
    std::cout << "[ProjectZelda64] restored OoT save snapshot: " << path.string() << '\n';
    return true;
}

bool TryRestoreOotSaveSnapshot() {
    const auto currentPath = std::filesystem::current_path();

    return TryRestoreOotSaveSnapshotFromPath(currentPath / kOotSaveSnapshotFileName) ||
           TryRestoreOotSaveSnapshotFromPath(currentPath / "x64" / "Release" / kOotSaveSnapshotFileName) ||
           TryRestoreOotSaveSnapshotFromPath(currentPath / "build" / "x64" / "Release" / kOotSaveSnapshotFileName) ||
           TryRestoreOotSaveSnapshotFromPath(currentPath / "extern" / "Shipwright" / "x64" / "Release" / kOotSaveSnapshotFileName) ||
           TryRestoreOotSaveSnapshotFromPath(currentPath / "extern" / "Shipwright" / "build" / "x64" / "Release" / kOotSaveSnapshotFileName);
}

void WriteSuppressNextPortalMarker() {
    const auto suppressPath = std::filesystem::current_path() / kSuppressNextPortalFileName;
    std::ofstream suppressMarker(suppressPath, std::ios::trunc);
    if (!suppressMarker.is_open()) {
        std::cout << "[ProjectZelda64] failed to write suppress marker: " << suppressPath.string() << '\n';
        return;
    }

    suppressMarker << "suppress next Happy Mask Shop portal after MM return\n";
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
        Contains(intent, "ENTR_HAPPY_MASK_SHOP_0") ||
        Contains(intent, "\"nativeEntranceIndex\": 1328") ||
        Contains(intent, "\"nativeEntranceIndex\":1328") ||
        Contains(intent, "Happy Mask");

    const bool isReturnFromMm =
        Contains(intent, "\"sourceGame\": \"mm\"") ||
        Contains(intent, "\"sourceGame\":\"mm\"") ||
        Contains(intent, "mm.enter_clock_tower_door") ||
        Contains(intent, "Clock Tower");

    if (!isOot || !isHappyMaskTarget || !isReturnFromMm) {
        return false;
    }

    std::error_code removeError;
    std::filesystem::remove(path, removeError);

    WriteSuppressNextPortalMarker();
    std::cout << "[ProjectZelda64] consumed OoT return launch intent: " << path.string() << '\n';
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
    TryRestoreOotSaveSnapshot();

    // ProjectZelda64: this launch intent came from MM returning to OoT.
    // Suppress the Happy Mask Shop portal once so OoT does not immediately route back to MM.
    CVarSetInteger("gProjectZelda64.SuppressHappyMaskPortal", 1);

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
    gSaveContext.respawnFlag = 0;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = kHappyMaskShopEntrance;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].roomIndex = 0;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].playerParams = 0xDFF;

    for (int buttonIndex = 0; buttonIndex < ARRAY_COUNT(gSaveContext.buttonStatus); buttonIndex++) {
        gSaveContext.buttonStatus[buttonIndex] = BTN_ENABLED;
    }

    gWeatherMode = 0;
}

void BootToHappyMaskShop() {
    PrepareHappyMaskShopSaveState();

    if (gGameState != nullptr) {
        gGameState->running = false;
        SET_NEXT_GAMESTATE(gGameState, Play_Init, PlayState);
    }

    GameInteractor_ExecuteOnLoadGame(gSaveContext.fileNum);
    std::cout << "[ProjectZelda64] booting OoT directly into Happy Mask Shop\n";
}

void CheckLaunchIntentOnFrame() {
    if (gPlayState != nullptr) {
        return;
    }

    if (TryConsumeHappyMaskLaunchIntent()) {
        BootToHappyMaskShop();
    }
}

void RegisterProjectZelda64LaunchIntent() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(CheckLaunchIntentOnFrame);
}

static RegisterShipInitFunc initFunc(RegisterProjectZelda64LaunchIntent);

} // namespace