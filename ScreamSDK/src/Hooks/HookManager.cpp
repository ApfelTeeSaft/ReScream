#include "ScreamSDK/Hooks.hpp"

#include "Internal.hpp"
#include "ScreamSDK/Log.hpp"
#include "ScreamSDK/Memory.hpp"

#include <MinHook.h>

#include <mutex>

namespace Scream {
namespace Hooks {

namespace {

std::mutex g_mutex;
std::vector<Status> g_status;
bool g_initialized = false;

Status* Find(const char* name) {
    for (std::size_t i = 0; i < g_status.size(); ++i) {
        if (g_status[i].name == name) {
            return &g_status[i];
        }
    }
    return NULL;
}

bool DisableAllLocked() {
    if (!g_initialized) {
        return true;
    }
    const MH_STATUS disabled = MH_DisableHook(MH_ALL_HOOKS);
    if (disabled != MH_OK && disabled != MH_ERROR_NOT_CREATED) {
        SCREAM_LOG_ERROR("Could not disable every hook: %s", MH_StatusToString(disabled));
        return false;
    }
    for (std::size_t i = 0; i < g_status.size(); ++i) {
        g_status[i].enabled = false;
    }
    return true;
}

} // namespace

bool Initialize() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_initialized) {
        return true;
    }
    const MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        SCREAM_LOG_ERROR("MinHook initialization failed: %s", MH_StatusToString(status));
        return false;
    }
    g_initialized = true;
    SCREAM_LOG_INFO("Hook engine initialized");
    return true;
}

bool Create(const char* name, void* target, void* detour, void** original) {
    return InternalHooks::Create(name, target, detour, original);
}

bool DisableAll() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return DisableAllLocked();
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized) {
        return;
    }
    if (!DisableAllLocked()) {
        SCREAM_LOG_ERROR("Hook removal was deferred because detours remain enabled");
        return;
    }
    for (std::size_t i = 0; i < g_status.size(); ++i) {
        if (!g_status[i].created || g_status[i].target == 0) {
            continue;
        }
        const MH_STATUS removed = MH_RemoveHook(
            reinterpret_cast<void*>(g_status[i].target));
        if (removed != MH_OK && removed != MH_ERROR_NOT_CREATED) {
            SCREAM_LOG_WARN("Could not remove hook %s: %s", g_status[i].name.c_str(),
                            MH_StatusToString(removed));
        }
        g_status[i].created = false;
    }
    const MH_STATUS uninitialized = MH_Uninitialize();
    if (uninitialized != MH_OK && uninitialized != MH_ERROR_NOT_INITIALIZED) {
        SCREAM_LOG_WARN("MinHook shutdown failed: %s", MH_StatusToString(uninitialized));
    }
    g_initialized = false;
}

std::vector<Status> Snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_status;
}

} // namespace Hooks

namespace InternalHooks {

bool Create(const char* name, void* target, void* detour, void** original) {
    std::lock_guard<std::mutex> lock(Hooks::g_mutex);
    Hooks::Status* existing = Hooks::Find(name);
    if (existing != NULL) {
        return existing->enabled;
    }

    Hooks::Status entry;
    entry.name = name;
    entry.target = reinterpret_cast<std::uintptr_t>(target);
    entry.created = false;
    entry.enabled = false;

    if (!Hooks::g_initialized) {
        entry.error = "hook engine is not initialized";
        Hooks::g_status.push_back(entry);
        return false;
    }
    if (target == NULL || detour == NULL || original == NULL ||
        !Memory::IsExecutable(target) || !Memory::IsAddressInAnyModule(target)) {
        entry.error = "target is not executable code in a loaded module";
        Hooks::g_status.push_back(entry);
        SCREAM_LOG_ERROR("Rejected hook %s at %p: %s", name, target, entry.error.c_str());
        return false;
    }

    MH_STATUS status = MH_CreateHook(target, detour, original);
    if (status != MH_OK) {
        entry.error = MH_StatusToString(status);
        Hooks::g_status.push_back(entry);
        SCREAM_LOG_ERROR("Could not create hook %s at %p: %s", name, target,
                         entry.error.c_str());
        return false;
    }
    entry.created = true;
    status = MH_EnableHook(target);
    if (status != MH_OK) {
        entry.error = MH_StatusToString(status);
        MH_RemoveHook(target);
        entry.created = false;
        Hooks::g_status.push_back(entry);
        SCREAM_LOG_ERROR("Could not enable hook %s at %p: %s", name, target,
                         entry.error.c_str());
        return false;
    }

    entry.enabled = true;
    Hooks::g_status.push_back(entry);
    SCREAM_LOG_INFO("Installed hook %-28s target=%p", name, target);
    return true;
}

bool IsEnabled(const char* name) {
    std::lock_guard<std::mutex> lock(Hooks::g_mutex);
    Hooks::Status* status = Hooks::Find(name);
    return status != NULL && status->enabled;
}

} // namespace InternalHooks
} // namespace Scream
