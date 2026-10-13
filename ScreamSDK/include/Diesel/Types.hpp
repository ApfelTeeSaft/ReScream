#pragma once

#include "ScreamSDK/Platform.hpp"

#include <cstddef>
#include <cstdint>

namespace Diesel {

struct Application;
struct Context;
struct World;
struct Input;
struct Unit;
struct UnitSlot;
struct UnitManager;
struct UnitHighlighter;
struct Camera;
struct CameraController;
struct Director;
struct WorldViewPort;
struct RenderInterface;
struct D3DRenderDevice;
struct LuaInterpreter;
struct VirtualFileSystem;
struct MainDatabase;

template <typename T>
struct ComPtr32 {
    T* pointer;
    std::uint32_t unknown_04;
};

template <typename T>
struct OpaqueMsvc9Vector32 {
    std::uint8_t storage[0x18];
};

template <typename T>
struct OpaqueMsvc9Set32 {
    std::uint8_t storage[0x20];
};

struct OpaqueMsvc9String32 {
    std::uint8_t storage[0x1C];
};

template <typename T>
struct WeakPointer32 {
    std::int32_t id;
};

static_assert(sizeof(ComPtr32<void>) == 8, "Diesel ComPtr is 8 bytes");
static_assert(sizeof(OpaqueMsvc9Vector32<void*>) == 0x18,
              "MSVC9 checked vector storage size");
static_assert(sizeof(OpaqueMsvc9Set32<std::uint32_t>) == 0x20,
              "MSVC9 checked set storage size");
static_assert(sizeof(OpaqueMsvc9String32) == 0x1C,
              "MSVC9 string storage size");
static_assert(sizeof(WeakPointer32<void>) == 0x04,
              "Diesel weak pointer storage size");

} // namespace Diesel
