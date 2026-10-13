#pragma once

#include <cstdint>

namespace Scream {
namespace Addresses {

// Singleton pointer storage slots
static const std::uintptr_t VirtualFileSystem = 0x007FB234;
static const std::uintptr_t MainDatabase = 0x007FB23C;
static const std::uintptr_t Application = 0x007FB24C;
static const std::uintptr_t Context = 0x007FB250;
static const std::uintptr_t World = 0x007FB254;
static const std::uintptr_t Input = 0x007FBC9C;
static const std::uintptr_t RenderingEnvironment = 0x007FC2CC;
static const std::uintptr_t Diesel = 0x007FDC94;
static const std::uintptr_t FileRequestManager = 0x007FDCB8;
static const std::uintptr_t LinearFileManager = 0x007FDCC4;
static const std::uintptr_t LuaRemoteDebugger = 0x00809BA8;

// Main loop and renderer
static const std::uintptr_t ApplicationUpdate = 0x001484B0;
static const std::uintptr_t ApplicationRun = 0x0014B360;
static const std::uintptr_t WorldUpdate = 0x00321980;
static const std::uintptr_t D3DRenderDevicePresent = 0x00528710;

// Lua owner globals
static const std::uintptr_t LuaInterpreterFor = 0x00004460;
static const std::uintptr_t LuaInterpreterMain = 0x0080E330;
static const std::uintptr_t LuaLoadingInterpreter = 0x0080E334;
static const std::uintptr_t LuaInterpreterVector = 0x0080E390;
static const std::uintptr_t LuaInterpreterFirst = 0x00810418;

// linked Lua C ABI
static const std::uintptr_t LuaGetTop = 0x004BBFE0;
static const std::uintptr_t LuaSetTop = 0x004BBFF0;
static const std::uintptr_t LuaRemove = 0x004BC040;
static const std::uintptr_t LuaInsert = 0x004BC080;
static const std::uintptr_t LuaPushValue = 0x004BC0C0;
static const std::uintptr_t LuaType = 0x004BC0F0;
static const std::uintptr_t LuaTypeName = 0x004BC110;
static const std::uintptr_t LuaToBoolean = 0x004BC1C0;
static const std::uintptr_t LuaToPointer = 0x004BC240;
static const std::uintptr_t LuaPushNil = 0x004BC2B0;
static const std::uintptr_t LuaPushNumber = 0x004BC2D0;
static const std::uintptr_t LuaPushBoolean = 0x004BC310;
static const std::uintptr_t LuaIsNumber = 0x004BF460;
static const std::uintptr_t LuaToNumber = 0x004BF500;
static const std::uintptr_t LuaToInteger = 0x004BF560;
static const std::uintptr_t LuaRawGet = 0x004BF630;
static const std::uintptr_t LuaLRef = 0x004C10B0;
static const std::uintptr_t LuaLUnref = 0x004C1190;
static const std::uintptr_t LuaToString = 0x004C1940;
static const std::uintptr_t LuaPushString = 0x004C42A0;
static const std::uintptr_t LuaPushLString = 0x004C4240;
static const std::uintptr_t LuaGetField = 0x004C42E0;
static const std::uintptr_t LuaSetField = 0x004C4340;
static const std::uintptr_t LuaNext = 0x004C43A0;
static const std::uintptr_t LuaRawGetI = 0x004BF660;
static const std::uintptr_t LuaRawSet = 0x004BF6A0;
static const std::uintptr_t LuaRawSetI = 0x004C1010;
static const std::uintptr_t LuaPCall = 0x004C7D50;
static const std::uintptr_t LuaPushCClosure = 0x004D10A0;
static const std::uintptr_t LuaLoad = 0x004D6CB0;
static const std::uintptr_t LuaLLoadBuffer = 0x004D6F40;

} // namespace Addresses
} // namespace Scream
