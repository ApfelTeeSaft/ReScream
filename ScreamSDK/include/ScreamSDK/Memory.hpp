#pragma once

#include "ScreamSDK/Platform.hpp"

#include <Windows.h>

#include <cstddef>
#include <cstdint>

namespace Scream {
namespace Memory {

SCREAM_API bool IsReadable(const void* address, std::size_t size);
SCREAM_API bool IsExecutable(const void* address);
SCREAM_API bool IsAddressInGame(const void* address);
SCREAM_API bool IsAddressInAnyModule(const void* address);

template <typename T>
bool TryRead(const void* address, T& value) {
    if (!IsReadable(address, sizeof(T))) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
        value = *static_cast<const T*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    value = *static_cast<const T*>(address);
    return true;
#endif
}

} // namespace Memory
} // namespace Scream
