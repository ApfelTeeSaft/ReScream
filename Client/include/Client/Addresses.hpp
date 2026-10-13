#pragma once

#include <cstdint>

namespace Client {
namespace Addresses {

static const std::uintptr_t InputUpdate = 0x000AD980;
static const std::uintptr_t LuaInterpreterMain = 0x0080E330;
static const std::uintptr_t Application = 0x007FB24C;

static const std::uintptr_t SteamSocketSendTo = 0x001D78F0;
static const std::uintptr_t SteamSocketRecvFrom = 0x001D7970;

static const std::uintptr_t LuaGetTop = 0x004BBFE0;
static const std::uintptr_t LuaSetTop = 0x004BBFF0;
static const std::uintptr_t LuaType = 0x004BC0F0;
static const std::uintptr_t LuaToBoolean = 0x004BC1C0;
static const std::uintptr_t LuaPushBoolean = 0x004BC310;
static const std::uintptr_t LuaToString = 0x004C1940;
static const std::uintptr_t LuaPushLString = 0x004C4240;
static const std::uintptr_t LuaGetField = 0x004C42E0;
static const std::uintptr_t LuaSetField = 0x004C4340;
static const std::uintptr_t LuaPCall = 0x004C7D50;
static const std::uintptr_t LuaLLoadBuffer = 0x004D6F40;

} // namespace Addresses
} // namespace Client
