#pragma once

#include "Diesel/Types.hpp"

namespace Diesel {
namespace Core {

SCREAM_API Application* GetApplication();
SCREAM_API Context* GetContext();
SCREAM_API World* GetWorld();
SCREAM_API Input* GetInput();
SCREAM_API VirtualFileSystem* GetVirtualFileSystem();
SCREAM_API MainDatabase* GetMainDatabase();
SCREAM_API void* GetDiesel();

} // namespace Core
} // namespace Diesel
