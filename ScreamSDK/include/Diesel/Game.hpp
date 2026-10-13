#pragma once

#include "Diesel/Math.hpp"
#include "Diesel/Types.hpp"

#include <Windows.h>

#include <cstddef>
#include <cstdint>

namespace Diesel {

#pragma pack(push, 1)
struct Application {
    std::uint8_t unknown_000[0x098];
    Input* input;                         // +0x098
    std::uint8_t unknown_09C[0x00C];
    void* dieselEngine;                  // +0x0A8
    std::uint8_t unknown_0AC[0x010];
    void* unitDatabase;                  // +0x0BC
    std::uint8_t unknown_0C0[0x084];
    World* world;                        // +0x144
    MainDatabase* database;              // +0x148
    std::uint8_t unknown_14C[0x0A0];
    RenderInterface* renderInterface;    // +0x1EC
    std::uint8_t unknown_1F0[0x040];
    LuaInterpreter* lua;                 // +0x230
    std::uint8_t unknown_234[0x0D8];
    HINSTANCE instance;                  // +0x30C
    std::uint8_t unknown_310[0x008];
    void* windowParameters;              // +0x318
    std::uint8_t unknown_31C[0x074];
};

struct Context {
    std::uint8_t unknown_000[0x219];
    bool useNilRenderDevice;             // +0x219
    std::uint8_t unknown_21A[0x5BB];
    bool threadedRenderer;               // +0x7D5
    std::uint8_t unknown_7D6[0x0D2];
};

struct World {
    std::uint8_t unknown_000[0x428];
    UnitManager* unitManager;            // +0x428
    std::uint8_t unknown_42C[0x0E8];
};

struct Unit {
    std::uint8_t unknown_000[0x15C];
    Vector3 position;                    // +0x15C
    std::uint8_t unknown_168[0x00C];
    Quaternion rotation;                 // +0x174
    std::uint8_t unknown_184[0x080];
    std::uint32_t id;                    // +0x204
    std::uint8_t unknown_208[0x004];
    bool enabled;                        // +0x20C
    std::uint8_t unknown_20D[0x057];
};

struct UnitManager {
    std::uint8_t unknown_000[0x098];
    World* owner;                                  // +0x098
    OpaqueMsvc9Vector32<Unit*> units;              // +0x09C
    OpaqueMsvc9Vector32<Unit*> enabledUnits;       // +0x0B4
    OpaqueMsvc9Vector32<Unit*> updatedUnits;       // +0x0CC
    OpaqueMsvc9Vector32<Unit*> slotChangeUnits;    // +0x0E4
    OpaqueMsvc9Vector32<Unit*> delayedPreStepUnits;// +0x0FC
    UnitSlot* slots[64];                           // +0x114
    std::int32_t checkMovingUnit;                  // +0x214
    UnitHighlighter* unitHighlighter;              // +0x218
    OpaqueMsvc9Set32<std::uint32_t> retainedUnitIds;// +0x21C; dsl::idstring
};

struct CameraVectors {
    Vector3 camera;                       // +0x00
    Vector3 target;                       // +0x0C
    Vector3 up;                           // +0x18
};

struct CameraController {
    std::uint8_t unknown_000[0x02C];
    Director* director;                   // +0x02C
    Camera* camera;                       // +0x030
    OpaqueMsvc9String32 name;             // +0x034
    CameraVectors cameraVectors;          // +0x050
};

struct Director {
    std::uint8_t unknown_000[0x058];
    OpaqueMsvc9Vector32<CameraController*> controllers; // +0x058
    CameraController* activeController;   // +0x070
    std::uint8_t blendedControllers[0x018];// +0x074; vector<BlendedController>
    std::uint8_t shakeMixer[0x064];        // +0x08C; CameraShakeMixer
    std::uint8_t updateCallback[0x00C];    // +0x0F0; dsl::LuaRef
    CameraVectors cameraVectors;          // +0x0FC
};

struct WorldViewPort {
    std::uint8_t unknown_000[0x0F4];       // ApplicationViewPort + ScriptReference
    WeakPointer32<Camera> camera;          // +0x0F4; weak object id, not Camera*
    Director* director;                   // +0x0F8
    bool listenerActive;                  // +0x0FC
    std::uint8_t padding_0FD[3];
    std::int32_t listenerMode;             // +0x100; enum underlying storage
    Unit* listenerUnit;                   // +0x104
    std::uint8_t unknown_108[0x008];
    std::uint8_t listenerObject[0x190];    // +0x110; opaque dsl::Object3D
};
#pragma pack(pop)

static_assert(offsetof(Application, input) == 0x098, "Application::_input");
static_assert(offsetof(Application, world) == 0x144, "Application::_world");
static_assert(offsetof(Application, renderInterface) == 0x1EC,
              "Application::_render_interface");
static_assert(offsetof(Application, lua) == 0x230, "Application::_lua");
static_assert(offsetof(Application, instance) == 0x30C, "Application::_instance");
static_assert(offsetof(Application, windowParameters) == 0x318,
              "Application::_window_params");
static_assert(sizeof(Application) == 0x390, "Application size");
static_assert(offsetof(Context, useNilRenderDevice) == 0x219,
              "Context::_use_nil_render_device");
static_assert(offsetof(Context, threadedRenderer) == 0x7D5,
              "Context::_threaded_renderer");
static_assert(sizeof(Context) == 0x8A8, "Context size");
static_assert(offsetof(World, unitManager) == 0x428, "World::_unit_manager");
static_assert(sizeof(World) == 0x514, "World size");
static_assert(offsetof(Unit, id) == 0x204, "Unit::_id");
static_assert(offsetof(Unit, position) == 0x15C, "Unit::_position");
static_assert(offsetof(Unit, rotation) == 0x174, "Unit::_rotation");
static_assert(offsetof(Unit, enabled) == 0x20C, "Unit::_enabled");
static_assert(sizeof(Unit) == 0x264, "Unit size");
static_assert(offsetof(UnitManager, owner) == 0x098, "UnitManager::_owner");
static_assert(offsetof(UnitManager, units) == 0x09C, "UnitManager::_units");
static_assert(offsetof(UnitManager, enabledUnits) == 0x0B4,
              "UnitManager::_enabled_units");
static_assert(offsetof(UnitManager, updatedUnits) == 0x0CC,
              "UnitManager::_updated_units");
static_assert(offsetof(UnitManager, slotChangeUnits) == 0x0E4,
              "UnitManager::_slot_change_units");
static_assert(offsetof(UnitManager, delayedPreStepUnits) == 0x0FC,
              "UnitManager::_delayed_pre_step_update_units");
static_assert(offsetof(UnitManager, slots) == 0x114, "UnitManager::_slots");
static_assert(offsetof(UnitManager, checkMovingUnit) == 0x214,
              "UnitManager::_check_moving_unit");
static_assert(offsetof(UnitManager, unitHighlighter) == 0x218,
              "UnitManager::_unit_highlighter");
static_assert(offsetof(UnitManager, retainedUnitIds) == 0x21C,
              "UnitManager::_retained_units");
static_assert(sizeof(UnitManager) == 0x23C, "UnitManager size");
static_assert(sizeof(CameraVectors) == 0x24, "CameraVectors size");
static_assert(offsetof(CameraController, director) == 0x2C,
              "CameraController::_director");
static_assert(offsetof(CameraController, camera) == 0x30,
              "CameraController::_camera");
static_assert(offsetof(CameraController, name) == 0x34,
              "CameraController::_name");
static_assert(offsetof(CameraController, cameraVectors) == 0x50,
              "CameraController::_cv");
static_assert(sizeof(CameraController) == 0x74, "CameraController size");
static_assert(offsetof(Director, controllers) == 0x58,
              "Director::_controllers");
static_assert(offsetof(Director, activeController) == 0x70,
              "Director::_active_controller");
static_assert(offsetof(Director, cameraVectors) == 0xFC,
              "Director::_camera_vectors");
static_assert(sizeof(Director) == 0x120, "Director size");
static_assert(offsetof(WorldViewPort, camera) == 0xF4,
              "WorldViewPort::_camera");
static_assert(offsetof(WorldViewPort, director) == 0xF8,
              "WorldViewPort::_director");
static_assert(offsetof(WorldViewPort, listenerActive) == 0xFC,
              "WorldViewPort::_listener_active");
static_assert(offsetof(WorldViewPort, listenerMode) == 0x100,
              "WorldViewPort::_listener_mode");
static_assert(offsetof(WorldViewPort, listenerUnit) == 0x104,
              "WorldViewPort::_listener_unit");
static_assert(offsetof(WorldViewPort, listenerObject) == 0x110,
              "WorldViewPort::_listener");
static_assert(sizeof(WorldViewPort) == 0x2A0, "WorldViewPort size");

namespace Game {

SCREAM_API UnitManager* GetUnitManager();
SCREAM_API World* GetUnitManagerOwner(UnitManager* manager);
SCREAM_API UnitSlot* GetUnitSlot(UnitManager* manager, std::size_t index);
SCREAM_API CameraController* GetActiveCameraController(Director* director);
SCREAM_API Director* GetCameraControllerDirector(CameraController* controller);
SCREAM_API Camera* GetCameraControllerCamera(CameraController* controller);
SCREAM_API Director* GetWorldViewPortDirector(WorldViewPort* viewPort);
SCREAM_API Unit* GetWorldViewPortListenerUnit(WorldViewPort* viewPort);
SCREAM_API bool TryGetWorldViewPortCameraId(WorldViewPort* viewPort,
                                            std::int32_t& id);

} // namespace Game

} // namespace Diesel
