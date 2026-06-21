#include <filesystem>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "variables.h"
}

namespace {

constexpr const char* kPortalEventFileName = "projectzelda64_portal_event.json";
constexpr const char* kSuppressNextPortalFileName = "projectzelda64_suppress_next_oot_portal.txt";
constexpr int32_t kHappyMaskShopEntrance = 0x0530;

bool ConsumeReturnGuardMarker() {
    const auto markerPath = std::filesystem::current_path() / kSuppressNextPortalFileName;

    std::error_code existsError;
    if (!std::filesystem::exists(markerPath, existsError) || existsError) {
        return false;
    }

    std::error_code removeError;
    std::filesystem::remove(markerPath, removeError);
    return true;
}

void RemovePortalEventIfPresent() {
    const auto eventPath = std::filesystem::current_path() / kPortalEventFileName;

    std::error_code existsError;
    if (!std::filesystem::exists(eventPath, existsError) || existsError) {
        return;
    }

    std::error_code removeError;
    std::filesystem::remove(eventPath, removeError);
}

void GuardReturnPortalOnFrame() {
    if (gSaveContext.entranceIndex != kHappyMaskShopEntrance) {
        return;
    }

    if (ConsumeReturnGuardMarker()) {
        RemovePortalEventIfPresent();
    }
}

void RegisterProjectZelda64ReturnPortalGuard() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>(GuardReturnPortalOnFrame);
}

static RegisterShipInitFunc initFunc(RegisterProjectZelda64ReturnPortalGuard);

} // namespace
