#include <libultraship/bridge/consolevariablebridge.h>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>

#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "global.h"
#include "variables.h"
}

namespace {
constexpr const char* kEnableOoTPortalsCVar = "gProjectZelda64.EnableOoTPortals";
constexpr const char* kSuppressHappyMaskPortalCVar = "gProjectZelda64.SuppressHappyMaskPortal";
constexpr const char* kPortalEventFileName = "projectzelda64_portal_event.json";
constexpr const char* kOotSaveSnapshotFileName = "projectzelda64_oot_save_snapshot.bin";

constexpr uint16_t kProjectZelda64DevicePromptTextId = 0x71F0;
constexpr uint16_t kProjectZelda64DeviceDeclineTextId = 0x71F1;

// OoT entrance index for the Happy Mask Shop interior.
constexpr int32_t kHappyMaskShopEntrance = 0x0530;

void WriteOotSaveSnapshot() {
    const std::filesystem::path snapshotPath = std::filesystem::current_path() / kOotSaveSnapshotFileName;
    std::ofstream snapshotFile(snapshotPath, std::ios::binary | std::ios::trunc);

    if (!snapshotFile.is_open()) {
        SPDLOG_WARN("ProjectZelda64: failed to open OoT save snapshot file: {}", snapshotPath.string());
        return;
    }

    snapshotFile.write(reinterpret_cast<const char*>(&gSaveContext), sizeof(gSaveContext));
    SPDLOG_INFO("ProjectZelda64: wrote OoT save snapshot to {}", snapshotPath.string());
}

void WritePortalEventFile(bool honorSuppressFlag) {
    if (honorSuppressFlag && CVarGetInteger(kSuppressHappyMaskPortalCVar, 0)) {
        CVarSetInteger(kSuppressHappyMaskPortalCVar, 0);
        SPDLOG_INFO("ProjectZelda64: suppressed one Happy Mask Shop portal after MM return");
        return;
    }

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

    SPDLOG_INFO("ProjectZelda64: wrote OoT salesman portal event to {}", eventPath.string());
}

void BuildProjectZelda64DevicePrompt(uint16_t* textId, bool* loadFromMessageTable) {
    CustomMessage msg = CustomMessage(
        "You look like an adventurer, young man. Last week, a strange man delivered to me a device unlike any I have seen before. Do you have the courage to use it on yourself and see what wonders it provides?\x1B%gYes&No%w",
        "You look like an adventurer, young man. Last week, a strange man delivered to me a device unlike any I have seen before. Do you have the courage to use it on yourself and see what wonders it provides?\x1B%gYes&No%w",
        "You look like an adventurer, young man. Last week, a strange man delivered to me a device unlike any I have seen before. Do you have the courage to use it on yourself and see what wonders it provides?\x1B%gYes&No%w");
    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void BuildProjectZelda64DeviceDecline(uint16_t* textId, bool* loadFromMessageTable) {
    CustomMessage msg = CustomMessage(
        "That is too bad. Come back again later if you change your mind.",
        "That is too bad. Come back again later if you change your mind.",
        "That is too bad. Come back again later if you change your mind.");
    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void RegisterProjectZelda64PortalBridge() {
    // CVar values can persist between Shipwright sessions. The suppress flag is only meant to be
    // an in-process one-shot set by a ProjectZelda64 MM->OoT launch intent.
    CVarSetInteger(kSuppressHappyMaskPortalCVar, 0);

    // No automatic portal on shop entry anymore. The salesman dialog owns the portal trigger.
    COND_ID_HOOK(OnOpenText, kProjectZelda64DevicePromptTextId, CVarGetInteger(kEnableOoTPortalsCVar, 1),
                 BuildProjectZelda64DevicePrompt);
    COND_ID_HOOK(OnOpenText, kProjectZelda64DeviceDeclineTextId, CVarGetInteger(kEnableOoTPortalsCVar, 1),
                 BuildProjectZelda64DeviceDecline);
}
} // namespace

extern "C" void ProjectZelda64_WriteHappyMaskSalesmanPortalEvent(void) {
    // The suppress flag only mattered for the old automatic entry trigger.
    // A deliberate salesman "Yes" should always write the portal event.
    CVarSetInteger(kSuppressHappyMaskPortalCVar, 0);
    WriteOotSaveSnapshot();
    WritePortalEventFile(false);
}

static RegisterShipInitFunc initFunc(RegisterProjectZelda64PortalBridge, { kEnableOoTPortalsCVar });