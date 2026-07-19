#include <libultraship/bridge/consolevariablebridge.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <spdlog/spdlog.h>
#include <vector>

#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/OTRGlobals.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "global.h"
#include "variables.h"
GetItemEntry ItemTable_Retrieve(int16_t getItemID);
}

namespace {
constexpr const char* kEnableOoTPortalsCVar = "gProjectZelda64.EnableOoTPortals";
constexpr const char* kSuppressHappyMaskPortalCVar = "gProjectZelda64.SuppressHappyMaskPortal";
constexpr const char* kEnableGoronMaskOcarinaExperimentCVar =
    "gProjectZelda64.Experiment.FairyOcarinaGivesMmGoronMask";
constexpr const char* kPortalEventFileName = "projectzelda64_portal_event.json";
constexpr const char* kOotSaveSnapshotFileName = "projectzelda64_oot_save_snapshot.bin";
constexpr const char* kSharedRupeesFileName = "projectzelda64_shared_rupees.json";
constexpr const char* kSharedStateFileName = "projectzelda64_shared_state.json";

constexpr uint16_t kProjectZelda64DevicePromptTextId = 0x71F0;
constexpr uint16_t kProjectZelda64DeviceDeclineTextId = 0x71F1;
constexpr uint16_t kProjectZelda64GoronMaskTextId = 0x71F2;

// OoT entrance index for the Happy Mask Shop interior.
constexpr int32_t kHappyMaskShopEntrance = 0x0530;
bool gPendingGoronMaskReward = false;

void AddPathIfUnique(std::vector<std::filesystem::path>& paths, std::set<std::string>& seen,
                     const std::filesystem::path& path) {
    const auto key = path.lexically_normal().string();
    if (seen.insert(key).second) {
        paths.push_back(path.lexically_normal());
    }
}

std::vector<std::filesystem::path> SharedFilePaths(const char* fileName) {
    std::vector<std::filesystem::path> paths;
    std::set<std::string> seen;

    std::error_code currentPathError;
    auto base = std::filesystem::current_path(currentPathError);
    if (currentPathError) {
        return paths;
    }

    for (int depth = 0; depth < 8 && !base.empty(); depth++) {
        AddPathIfUnique(paths, seen, base / fileName);
        AddPathIfUnique(paths, seen, base / "x64" / "Release" / fileName);
        AddPathIfUnique(paths, seen, base / "build" / "x64" / "Release" / fileName);
        AddPathIfUnique(paths, seen, base / "extern" / "Shipwright" / "x64" / "Release" / fileName);
        AddPathIfUnique(paths, seen, base / "extern" / "Shipwright" / "build" / "x64" / "Release" / fileName);
        AddPathIfUnique(paths, seen, base / "extern" / "2ship2harkinian" / "x64" / "Release" / fileName);
        AddPathIfUnique(paths, seen, base / "extern" / "2ship2harkinian" / "build" / "x64" / "Release" / fileName);

        const auto parent = base.parent_path();
        if (parent == base) {
            break;
        }
        base = parent;
    }

    return paths;
}

void WriteJsonToSharedPaths(const char* fileName, const std::string& json) {
    bool wroteAny = false;

    for (const auto& path : SharedFilePaths(fileName)) {
        const auto parent = path.parent_path();
        std::error_code existsError;
        if (!parent.empty() && (!std::filesystem::exists(parent, existsError) || existsError)) {
            continue;
        }

        std::ofstream output(path, std::ios::trunc);
        if (!output.is_open()) {
            continue;
        }

        output << json;
        wroteAny = true;
        SPDLOG_INFO("ProjectZelda64: wrote shared state file {}", path.string());
    }

    if (!wroteAny) {
        SPDLOG_WARN("ProjectZelda64: failed to write shared state file {} to any known path", fileName);
    }
}

void WriteSharedRupees() {
    WriteJsonToSharedPaths(kSharedRupeesFileName,
                           "{\n"
                           "  \"schema\": 1,\n"
                           "  \"sourceGame\": \"oot\",\n"
                           "  \"sharedRupees\": " + std::to_string(gSaveContext.rupees) + "\n"
                           "}\n");
}

void WriteGoronMaskSharedState() {
    WriteJsonToSharedPaths(kSharedStateFileName,
                           "{\n"
                           "  \"schema\": 1,\n"
                           "  \"sourceGame\": \"oot\",\n"
                           "  \"sourceEvent\": \"oot.lost_woods_bridge.fairy_ocarina\",\n"
                           "  \"sharedItems\": {\n"
                           "    \"mm.goron_mask\": true\n"
                           "  }\n"
                           "}\n");
    SPDLOG_INFO("ProjectZelda64: Fairy Ocarina reward redirected to MM Goron Mask shared state");
}

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
              << "  \"sharedRupees\": " << gSaveContext.rupees << ",\n"
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

void BuildProjectZelda64GoronMaskMessage(uint16_t* textId, bool* loadFromMessageTable) {
    CustomMessage msg = CustomMessage(
        "You received the %rGoron Mask%w!",
        "You received the %rGoron Mask%w!",
        "You received the %rGoron Mask%w!");
    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void TryGivePendingGoronMaskReward() {
    if (!gPendingGoronMaskReward || gPlayState == nullptr) {
        return;
    }

    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr || Player_InBlockingCsMode(gPlayState, player) ||
        player->stateFlags1 & (PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM |
                               PLAYER_STATE1_CARRYING_ACTOR)) {
        return;
    }

    GetItemEntry goronMaskEntry = ItemTable_Retrieve(GI_MASK_GORON);
    goronMaskEntry.textId = kProjectZelda64GoronMaskTextId;
    GiveItemEntryWithoutActor(gPlayState, goronMaskEntry);
}

void OnProjectZelda64ItemReceive(GetItemEntry itemEntry) {
    if (!gPendingGoronMaskReward || itemEntry.modIndex != MOD_NONE || itemEntry.getItemId != GI_MASK_GORON) {
        return;
    }

    gPendingGoronMaskReward = false;
    WriteGoronMaskSharedState();
}

void RegisterProjectZelda64PortalBridge() {
    // CVar values can persist between Shipwright sessions. The suppress flag is only meant to be
    // an in-process one-shot set by a ProjectZelda64 MM->OoT launch intent.
    CVarSetInteger(kSuppressHappyMaskPortalCVar, 0);
    CVarSetInteger(kEnableGoronMaskOcarinaExperimentCVar, 1);

    COND_VB_SHOULD(VB_GIVE_ITEM_FAIRY_OCARINA, CVarGetInteger(kEnableGoronMaskOcarinaExperimentCVar, 1), {
        gPendingGoronMaskReward = true;
        *should = false;
    });
    COND_HOOK(OnPlayerUpdate, CVarGetInteger(kEnableGoronMaskOcarinaExperimentCVar, 1),
              TryGivePendingGoronMaskReward);
    COND_HOOK(OnItemReceive, CVarGetInteger(kEnableGoronMaskOcarinaExperimentCVar, 1),
              OnProjectZelda64ItemReceive);

    // No automatic portal on shop entry anymore. The salesman dialog owns the portal trigger.
    COND_ID_HOOK(OnOpenText, kProjectZelda64DevicePromptTextId, CVarGetInteger(kEnableOoTPortalsCVar, 1),
                 BuildProjectZelda64DevicePrompt);
    COND_ID_HOOK(OnOpenText, kProjectZelda64DeviceDeclineTextId, CVarGetInteger(kEnableOoTPortalsCVar, 1),
                 BuildProjectZelda64DeviceDecline);
    COND_ID_HOOK(OnOpenText, kProjectZelda64GoronMaskTextId,
                 CVarGetInteger(kEnableGoronMaskOcarinaExperimentCVar, 1), BuildProjectZelda64GoronMaskMessage);
}
} // namespace

extern "C" void ProjectZelda64_WriteHappyMaskSalesmanPortalEvent(void) {
    // The suppress flag only mattered for the old automatic entry trigger.
    // A deliberate salesman "Yes" should always write the portal event.
    CVarSetInteger(kSuppressHappyMaskPortalCVar, 0);
    Audio_PlaySoundGeneral(NA_SE_OC_SECRET_WARP_OUT, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                           &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    WriteOotSaveSnapshot();
    WriteSharedRupees();
    WritePortalEventFile(false);
}

static RegisterShipInitFunc initFunc(RegisterProjectZelda64PortalBridge,
                                     { kEnableOoTPortalsCVar, kEnableGoronMaskOcarinaExperimentCVar });
