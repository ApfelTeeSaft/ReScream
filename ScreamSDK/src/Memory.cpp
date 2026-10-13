#include "ScreamSDK/Memory.hpp"

#include "ScreamSDK/Build.hpp"

#include <Windows.h>

#include <limits>

namespace Scream {
namespace Memory {

namespace {

bool IsReadableProtection(DWORD protection) {
    const DWORD base = protection & 0xFFu;
    return base == PAGE_READONLY || base == PAGE_READWRITE ||
           base == PAGE_WRITECOPY || base == PAGE_EXECUTE_READ ||
           base == PAGE_EXECUTE_READWRITE || base == PAGE_EXECUTE_WRITECOPY;
}

bool IsExecutableProtection(DWORD protection) {
    const DWORD base = protection & 0xFFu;
    return base == PAGE_EXECUTE || base == PAGE_EXECUTE_READ ||
           base == PAGE_EXECUTE_READWRITE || base == PAGE_EXECUTE_WRITECOPY;
}

} // namespace

bool IsReadable(const void* address, std::size_t size) {
    if (address == NULL) {
        return false;
    }
    if (size == 0) {
        return true;
    }
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(address);
    if (size > std::numeric_limits<std::uintptr_t>::max() - start) {
        return false;
    }
    const std::uintptr_t end = start + size;
    std::uintptr_t cursor = start;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info = {};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info,
                         sizeof(info)) != sizeof(info)) {
            return false;
        }
        if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
            !IsReadableProtection(info.Protect)) {
            return false;
        }
        const std::uintptr_t regionStart = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        const std::uintptr_t regionEnd = regionStart + info.RegionSize;
        if (regionEnd <= cursor) {
            return false;
        }
        cursor = regionEnd;
    }
    return true;
}

bool IsExecutable(const void* address) {
    if (address == NULL) {
        return false;
    }
    MEMORY_BASIC_INFORMATION info = {};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info)) {
        return false;
    }
    return info.State == MEM_COMMIT && !(info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
           IsExecutableProtection(info.Protect);
}

bool IsAddressInGame(const void* address) {
    const std::uintptr_t base = Build::GameBase();
    const std::uint32_t size = Build::GameImageSize();
    const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(address);
    return base != 0 && size != 0 && value >= base && value - base < size;
}

bool IsAddressInAnyModule(const void* address) {
    if (address == NULL) {
        return false;
    }
    HMODULE module = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(address), &module)) {
        return false;
    }
    return module != NULL;
}

} // namespace Memory
} // namespace Scream
