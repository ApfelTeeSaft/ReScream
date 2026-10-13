#pragma once

#include "ScreamSDK/Platform.hpp"

#include <cstdint>

extern "C" {

SCREAM_API bool SCREAM_CDECL ScreamSDK_EnsureLoaded();
SCREAM_API void SCREAM_CDECL ScreamSDK_RequestShutdown();
SCREAM_API bool SCREAM_CDECL ScreamSDK_WaitForShutdown(std::uint32_t timeoutMilliseconds);
SCREAM_API bool SCREAM_CDECL ScreamSDK_IsShutdownComplete();
SCREAM_API std::uint32_t SCREAM_CDECL ScreamSDK_InterfaceVersion();

}
