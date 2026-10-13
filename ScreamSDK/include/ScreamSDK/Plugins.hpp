#pragma once

#include "ScreamSDK/Platform.hpp"

#include <cstdint>

static const std::uint32_t SCREAMSDK_OVERLAY_PANEL_ABI_V1 = 0x00010000u;

extern "C" {

typedef void (SCREAM_CDECL* ScreamSDK_OverlayDrawCallback)(void* userData);

struct ScreamSDK_OverlayPanelV1 {
    std::uint32_t structSize;
    std::uint32_t abiVersion;
    const char* id;
    const char* title;
    std::int32_t sortOrder;
    void* userData;
    ScreamSDK_OverlayDrawCallback draw;
};

SCREAM_API std::uint64_t SCREAM_CDECL ScreamSDK_RegisterOverlayPanel(
    const ScreamSDK_OverlayPanelV1* panel);

SCREAM_API bool SCREAM_CDECL ScreamSDK_UnregisterOverlayPanel(
    std::uint64_t handle);

SCREAM_API bool SCREAM_CDECL ScreamSDK_IsOverlayPanelRegistered(
    std::uint64_t handle);

}
