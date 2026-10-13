#pragma once

#include "ScreamSDK/Platform.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Scream {
namespace Hooks {

struct Status {
    std::string name;
    std::uintptr_t target;
    bool created;
    bool enabled;
    std::string error;
};

SCREAM_API bool Initialize();
SCREAM_API bool Create(const char* name,
                       void* target,
                       void* detour,
                       void** original);
SCREAM_API bool DisableAll();
SCREAM_API void Shutdown();
SCREAM_API std::vector<Status> Snapshot();

template <typename T>
bool Create(const char* name, T target, T detour, T& original) {
    return Create(name,
                  reinterpret_cast<void*>(target),
                  reinterpret_cast<void*>(detour),
                  reinterpret_cast<void**>(&original));
}

} // namespace Hooks
} // namespace Scream
