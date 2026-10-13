#include "Diesel/Core.hpp"

#include "Diesel/Game.hpp"

#include "ScreamSDK/Addresses.hpp"
#include "ScreamSDK/Build.hpp"
#include "ScreamSDK/Memory.hpp"

namespace {

template <typename T>
T* ReadSingleton(std::uintptr_t rva) {
    T* value = NULL;
    const void* slot = reinterpret_cast<const void*>(Scream::Build::GameBase() + rva);
    if (!Scream::Memory::TryRead(slot, value) || value == NULL ||
        !Scream::Memory::IsReadable(value, sizeof(void*))) {
        return NULL;
    }
    return value;
}

} // namespace

namespace Diesel {
namespace Core {

Application* GetApplication() {
    return ReadSingleton<Application>(Scream::Addresses::Application);
}

Context* GetContext() {
    return ReadSingleton<Context>(Scream::Addresses::Context);
}

World* GetWorld() {
    return ReadSingleton<World>(Scream::Addresses::World);
}

Input* GetInput() {
    return ReadSingleton<Input>(Scream::Addresses::Input);
}

VirtualFileSystem* GetVirtualFileSystem() {
    return ReadSingleton<VirtualFileSystem>(Scream::Addresses::VirtualFileSystem);
}

MainDatabase* GetMainDatabase() {
    return ReadSingleton<MainDatabase>(Scream::Addresses::MainDatabase);
}

void* GetDiesel() {
    return ReadSingleton<void>(Scream::Addresses::Diesel);
}

} // namespace Core

namespace Game {

namespace {

template <typename Owner, typename Value>
Value* ReadObjectPointer(Owner* owner, Value* const* member) {
    if (owner == NULL || member == NULL ||
        !Scream::Memory::IsReadable(member, sizeof(Value*))) {
        return NULL;
    }
    Value* value = NULL;
    if (!Scream::Memory::TryRead(member, value) || value == NULL ||
        !Scream::Memory::IsReadable(value, sizeof(void*))) {
        return NULL;
    }
    return value;
}

} // namespace

UnitManager* GetUnitManager() {
    World* world = Core::GetWorld();
    return world == NULL ? NULL : ReadObjectPointer(world, &world->unitManager);
}

World* GetUnitManagerOwner(UnitManager* manager) {
    return manager == NULL ? NULL : ReadObjectPointer(manager, &manager->owner);
}

UnitSlot* GetUnitSlot(UnitManager* manager, std::size_t index) {
    if (manager == NULL || index >= 64) {
        return NULL;
    }
    return ReadObjectPointer(manager, &manager->slots[index]);
}

CameraController* GetActiveCameraController(Director* director) {
    return director == NULL
        ? NULL : ReadObjectPointer(director, &director->activeController);
}

Director* GetCameraControllerDirector(CameraController* controller) {
    return controller == NULL
        ? NULL : ReadObjectPointer(controller, &controller->director);
}

Camera* GetCameraControllerCamera(CameraController* controller) {
    return controller == NULL
        ? NULL : ReadObjectPointer(controller, &controller->camera);
}

Director* GetWorldViewPortDirector(WorldViewPort* viewPort) {
    return viewPort == NULL
        ? NULL : ReadObjectPointer(viewPort, &viewPort->director);
}

Unit* GetWorldViewPortListenerUnit(WorldViewPort* viewPort) {
    return viewPort == NULL
        ? NULL : ReadObjectPointer(viewPort, &viewPort->listenerUnit);
}

bool TryGetWorldViewPortCameraId(WorldViewPort* viewPort, std::int32_t& id) {
    if (viewPort == NULL ||
        !Scream::Memory::TryRead(&viewPort->camera.id, id)) {
        return false;
    }
    return true;
}

} // namespace Game
} // namespace Diesel
