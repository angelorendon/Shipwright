#include <libultraship/bridge/consolevariablebridge.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <spdlog/spdlog.h>
#include <vector>
#include <ship/Context.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>

#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "global.h"
#include "variables.h"
GetItemEntry ItemTable_Retrieve(int16_t getItemID);
extern uint16_t gProjectZelda64MmGetMaskSequenceId;
uint16_t Audio_RegisterProjectZelda64MmGetMaskSequence(void);
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
constexpr const char* kMmGoronMaskDisplayList =
    "__OTR__objects/object_gi_golonmask/gGiGoronMaskDL";
constexpr const char* kMmGoronMaskEmptyDisplayList =
    "__OTR__objects/object_gi_golonmask/gGiGoronMaskEmptyDL";

// OoT entrance index for the Happy Mask Shop interior.
constexpr int32_t kHappyMaskShopEntrance = 0x0530;
bool gPendingGoronMaskReward = false;
bool gUseMmGoronMaskFanfare = false;
uint8_t gPreviousOotChildTradeItem = ITEM_NONE;

void PrioritizeProjectZelda64AssetPack() {
    auto archiveManager =
        Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    auto archives = archiveManager->GetArchives();
    std::shared_ptr<Ship::Archive> assetPack;

    for (const auto& archive : *archives) {
        if (std::filesystem::path(archive->GetPath()).filename() ==
            "projectzelda64-mm-goron-mask.o2r") {
            assetPack = archive;
            break;
        }
    }

    if (assetPack == nullptr) {
        SPDLOG_ERROR("ProjectZelda64: could not find the MM reward asset pack to prioritize");
        return;
    }

    // OoT's archive is mounted after the mods directory and otherwise wins all
    // colliding resource names. Move this pack to the end of the archive list.
    archiveManager->RemoveArchive(assetPack);
    archiveManager->AddArchive(assetPack);
    ResourceMgr_UnloadResource(kMmGoronMaskDisplayList);
    ResourceMgr_UnloadResource(kMmGoronMaskEmptyDisplayList);
    ResourceMgr_UnloadResource("__OTR__audio/sequences/034_Got_Key_Item");
    SPDLOG_INFO("ProjectZelda64: promoted MM reward asset pack above oot.o2r");
}

void DrawMmGoronMask(PlayState* play, GetItemEntry*) {
    static Gfx* displayList = nullptr;
    static Gfx* emptyDisplayList = nullptr;
    if (displayList == nullptr) {
        // This authentic MM resource has the same name as OoT's mask. Evict OoT's
        // cached version so archive priority can select the mod's MM resource.
        ResourceMgr_UnloadResource(kMmGoronMaskDisplayList);
        ResourceMgr_UnloadResource(kMmGoronMaskEmptyDisplayList);
        displayList = ResourceMgr_LoadGfxByName(kMmGoronMaskDisplayList);
        emptyDisplayList = ResourceMgr_LoadGfxByName(kMmGoronMaskEmptyDisplayList);
    }
    if (displayList != nullptr && emptyDisplayList != nullptr) {
        {
        GraphicsContext* __gfxCtx = play->state.gfxCtx;

        // MM defines the empty shell as opaque and the textured face as translucent.
        // Drawing only the face through OoT's opaque helper produces the corrupted
        // hybrid that was visible in the previous test.
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        gSPDisplayList(POLY_OPA_DISP++, emptyDisplayList);

        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, (char*)__FILE__, __LINE__),
                  G_MTX_MODELVIEW | G_MTX_LOAD);
        gSPDisplayList(POLY_XLU_DISP++, displayList);

        }
    } else {
        static bool loggedMissingModel = false;
        if (!loggedMissingModel) {
            SPDLOG_ERROR("ProjectZelda64: one or more MM Goron Mask display lists were not loaded");
            loggedMissingModel = true;
        }
    }
}

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
    goronMaskEntry.drawFunc = DrawMmGoronMask;
    gUseMmGoronMaskFanfare = true;
    GiveItemEntryWithoutActor(gPlayState, goronMaskEntry);
}

void OnProjectZelda64ItemReceive(GetItemEntry itemEntry) {
    if (!gPendingGoronMaskReward || itemEntry.modIndex != MOD_NONE || itemEntry.getItemId != GI_MASK_GORON) {
        return;
    }

    gPendingGoronMaskReward = false;
    // The OoT item is only a presentation vehicle for this cross-game reward. Keep OoT's
    // child-trade inventory exactly as it was; ProjectZelda64 grants the usable mask in MM.
    gSaveContext.inventory.items[SLOT_TRADE_CHILD] = gPreviousOotChildTradeItem;
    WriteGoronMaskSharedState();
}

void RegisterProjectZelda64PortalBridge() {
    PrioritizeProjectZelda64AssetPack();

    // CVar values can persist between Shipwright sessions. The suppress flag is only meant to be
    // an in-process one-shot set by a ProjectZelda64 MM->OoT launch intent.
    CVarSetInteger(kSuppressHappyMaskPortalCVar, 0);
    CVarSetInteger(kEnableGoronMaskOcarinaExperimentCVar, 1);

    COND_VB_SHOULD(VB_GIVE_ITEM_FAIRY_OCARINA, CVarGetInteger(kEnableGoronMaskOcarinaExperimentCVar, 1), {
        gPreviousOotChildTradeItem = gSaveContext.inventory.items[SLOT_TRADE_CHILD];
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

extern "C" uint16_t ProjectZelda64_ConsumeMmGoronMaskFanfare(void) {
    if (!gUseMmGoronMaskFanfare) {
        return 0;
    }
    gUseMmGoronMaskFanfare = false;
    if (gProjectZelda64MmGetMaskSequenceId == 0) {
        Audio_RegisterProjectZelda64MmGetMaskSequence();
    }
    if (gProjectZelda64MmGetMaskSequenceId == 0) {
        SPDLOG_ERROR("ProjectZelda64: MM Get Mask sequence was not registered from the mod archive");
    } else {
        SPDLOG_INFO("ProjectZelda64: playing MM Get Mask sequence {}", gProjectZelda64MmGetMaskSequenceId);
    }
    return gProjectZelda64MmGetMaskSequenceId;
}

static RegisterShipInitFunc initFunc(RegisterProjectZelda64PortalBridge,
                                     { kEnableOoTPortalsCVar, kEnableGoronMaskOcarinaExperimentCVar });
