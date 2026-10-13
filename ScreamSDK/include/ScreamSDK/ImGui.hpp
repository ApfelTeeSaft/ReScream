#pragma once

#include "ScreamSDK/Platform.hpp"

#if !defined(IMGUI_API)
#if defined(SCREAMSDK_EXPORTS)
#define IMGUI_API __declspec(dllexport)
#else
#define IMGUI_API __declspec(dllimport)
#endif
#endif

#include <imgui.h>

namespace Scream {
namespace UI {

SCREAM_API ImFont* InterfaceFont();
SCREAM_API ImFont* HeadingFont();
SCREAM_API ImFont* CodeFont();

SCREAM_API float DpiScale();

} // namespace UI
} // namespace Scream
