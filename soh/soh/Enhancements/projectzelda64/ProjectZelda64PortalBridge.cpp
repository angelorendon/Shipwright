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

void RegisterProjectZelda64PortalBridge() {
    // The OoT -> MM portal is now intentionally triggered by the Happy Mask Salesman's
    // custom Yes/No dialog in En_Ossan, not by merely entering the Happy Mask Shop scene.
    // Keep this registration stub so old docs/CVars still have a safe no-op endpoint.
    SPDLOG_INFO("ProjectZelda64: Happy Mask Shop entrance portal bridge disabled; talk to salesman to travel");
}
} // namespace

static RegisterShipInitFunc initFunc(RegisterProjectZelda64PortalBridge, { kEnableOoTPortalsCVar });
