#include <libultraship/bridge/consolevariablebridge.h>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "variables.h"
}

namespace {
constexpr const char* kEnableOoTPortalsCVar = "gProjectZelda64.EnableOoTPortals";
constexpr const char* kPortalEventFileName = "projectzelda64_portal_event.json";

// OoT entrance index for entering the Happy Mask Shop interior.
// ProjectZelda64 uses this as the first cross-game portal trigger:
// OoT Happy Mask Shop -> MM Clock Town / Clock Tower Door exterior.
constexpr int32_t kHappyMaskShopEntrance = 0x0530;

void WritePortalEventFile() {
    const std::filesystem::path eventPath = std::filesystem::current_path() / kPortalEventFileName;
    std::ofstream eventFile(eventPath, std::ios::trunc);

    if (!eventFile.is_open()) {
        SPDLOG_WARN("ProjectZelda64: failed to open portal event file: {}", eventPath.string());
        return;
    }

    eventFile << "{\n"
              << "  \"schema\": 1,\n"
              << "  \"sourceGame\": \"oot\",\n"
              << "  \"event\": \"oot.enter_happy_mask_shop\",\n"
              << "  \"sourceEntrance\": \"ENTR_HAPPY_MASK_SHOP_0\",\n"
              << "  \"sourceEntranceIndex\": " << kHappyMaskShopEntrance << ",\n"
              << "  \"targetGame\": \"mm\",\n"
              << "  \"targetPortal\": \"mm.clock_town.clock_tower_door_exterior\"\n"
              << "}\n";

    SPDLOG_INFO("ProjectZelda64: wrote OoT portal event to {}", eventPath.string());
}

void RegisterProjectZelda64PortalBridge() {
    COND_HOOK(OnSceneInit, CVarGetInteger(kEnableOoTPortalsCVar, 0), [](int16_t sceneNum) {
        // SceneInit runs after the normal entrance has already loaded. This is intentionally a safe first
        // integration step: it lets ProjectZelda64 detect the portal without changing vanilla Shipwright behavior yet.
        if (gSaveContext.entranceIndex == kHappyMaskShopEntrance) {
            SPDLOG_INFO("ProjectZelda64: detected OoT Happy Mask Shop portal scene={}, entrance=0x{:04X}", sceneNum,
                        gSaveContext.entranceIndex);
            WritePortalEventFile();
        }
    });
}
} // namespace

static RegisterShipInitFunc initFunc(RegisterProjectZelda64PortalBridge, { kEnableOoTPortalsCVar });
