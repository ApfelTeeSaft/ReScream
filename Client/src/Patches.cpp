#include "Patches.hpp"

#include "LuaFixes.hpp"
#include "Log.hpp"
#include "NativeQol.hpp"

#include <MinHook.h>

namespace Client {
namespace Patches {

namespace {

bool g_initialized = false;

} // namespace

bool Initialize() {
    if (g_initialized) {
        return true;
    }
    const MH_STATUS status = MH_Initialize();
    if (status != MH_OK) {
        CLIENT_LOG_ERROR("MinHook initialization failed: %s",
                         MH_StatusToString(status));
        return false;
    }
    g_initialized = true;
    if (!NativeQol::Install()) {
        MH_Uninitialize();
        g_initialized = false;
        return false;
    }
    if (!LuaFixes::Install()) {
        NativeQol::Remove();
        MH_Uninitialize();
        g_initialized = false;
        return false;
    }
    CLIENT_LOG_INFO(
        "ReScream client initialized!");
    return true;
}

bool Shutdown() {
    if (!g_initialized) {
        return true;
    }
    const MH_STATUS disable = MH_DisableHook(MH_ALL_HOOKS);
    if (disable != MH_OK && disable != MH_ERROR_NOT_CREATED) {
        CLIENT_LOG_ERROR("Could not disable client hooks: %s",
                         MH_StatusToString(disable));
        return false;
    }
    LuaFixes::Quiesce();
    if (!LuaFixes::Remove()) {
        return false;
    }
    if (!NativeQol::Remove()) {
        CLIENT_LOG_ERROR("Could not remove native quality-of-life hooks");
        return false;
    }
    const MH_STATUS uninitialize = MH_Uninitialize();
    if (uninitialize != MH_OK) {
        CLIENT_LOG_ERROR("Could not shut down MinHook: %s",
                         MH_StatusToString(uninitialize));
        return false;
    }
    g_initialized = false;
    return true;
}

} // namespace Patches
} // namespace Client
