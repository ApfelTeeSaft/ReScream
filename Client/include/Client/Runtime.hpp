#pragma once

#include "Client/Platform.hpp"

#include <cstdint>

extern "C" {

CLIENT_API std::uint32_t CLIENT_CDECL Client_InterfaceVersion();
CLIENT_API void CLIENT_CDECL Client_RequestShutdown();
CLIENT_API bool CLIENT_CDECL Client_WaitForShutdown(
    std::uint32_t timeoutMilliseconds);
CLIENT_API bool CLIENT_CDECL Client_IsShutdownComplete();
CLIENT_API bool CLIENT_CDECL Client_IsSupportedBuild();
CLIENT_API bool CLIENT_CDECL Client_AreFixesActive();
CLIENT_API bool CLIENT_CDECL Client_IsF4FixApplied();
CLIENT_API bool CLIENT_CDECL Client_IsKeybindFixApplied();
CLIENT_API bool CLIENT_CDECL Client_IsFovFeatureApplied();
CLIENT_API bool CLIENT_CDECL Client_IsPerformanceFeatureApplied();
CLIENT_API bool CLIENT_CDECL Client_IsQualityOfLifeFeatureApplied();
CLIENT_API bool CLIENT_CDECL Client_IsLobbyBrowserFixApplied();
CLIENT_API bool CLIENT_CDECL Client_IsVersionOverrideApplied();
CLIENT_API bool CLIENT_CDECL Client_IsStaticBundleProbeSeen();

}
