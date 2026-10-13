#include "Workbench.hpp"

#include "Diesel/Core.hpp"
#include "Diesel/Game.hpp"
#include "Diesel/Lua.hpp"
#include "Diesel/Renderer.hpp"
#include "Internal.hpp"
#include "ScreamSDK/Build.hpp"
#include "ScreamSDK/Hooks.hpp"
#include "ScreamSDK/Log.hpp"
#include "ScreamSDK/Memory.hpp"

#include <imgui.h>

#include <vector>

namespace Scream {
namespace Workbench {

namespace {

void PointerRow(const char* name, const void* pointer, std::size_t readableBytes) {
    void* firstWord = NULL;
    const bool readable = Memory::IsReadable(pointer, readableBytes);
    if (readable) {
        Memory::TryRead(pointer, firstWord);
    }
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(name);
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("0x%08X", static_cast<unsigned int>(
        reinterpret_cast<std::uintptr_t>(pointer)));
    ImGui::TableSetColumnIndex(2);
    ImGui::TextUnformatted(readable ? "yes" : "no");
    ImGui::TableSetColumnIndex(3);
    ImGui::Text("0x%08X", static_cast<unsigned int>(
        reinterpret_cast<std::uintptr_t>(firstWord)));
    ImGui::TableSetColumnIndex(4);
    ImGui::TextUnformatted(Memory::IsExecutable(firstWord) ? "yes" : "no");
}

bool BeginPointerTable(const char* id) {
    if (!ImGui::BeginTable(id, 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                 ImGuiTableFlags_Resizable)) {
        return false;
    }
    ImGui::TableSetupColumn("Object");
    ImGui::TableSetupColumn("Address");
    ImGui::TableSetupColumn("Readable");
    ImGui::TableSetupColumn("Vtable / first word");
    ImGui::TableSetupColumn("Executable");
    ImGui::TableHeadersRow();
    return true;
}

} // namespace

void DrawEngineTab() {
    Diesel::Application* application = Diesel::Core::GetApplication();
    Diesel::Context* context = Diesel::Core::GetContext();
    Diesel::World* world = Diesel::Core::GetWorld();
    Diesel::Input* input = Diesel::Core::GetInput();
    Diesel::VirtualFileSystem* vfs = Diesel::Core::GetVirtualFileSystem();
    Diesel::MainDatabase* database = Diesel::Core::GetMainDatabase();
    Diesel::RenderInterface* renderInterface = Diesel::Renderer::GetRenderInterface();
    Diesel::D3DRenderDevice* renderDevice = Diesel::Renderer::GetRenderDevice();
    IDirect3DDevice9* d3d = Diesel::Renderer::GetD3D9Device();
    Diesel::UnitManager* unitManager = Diesel::Game::GetUnitManager();
    Diesel::World* unitManagerOwner =
        Diesel::Game::GetUnitManagerOwner(unitManager);
    Diesel::LuaInterpreter* luaOwner = NULL;
    if (application != NULL) {
        Memory::TryRead(&application->lua, luaOwner);
    }

    ImGui::Text("Executable base: 0x%08X",
                static_cast<unsigned int>(Build::GameBase()));
    ImGui::Text("Executable image size: 0x%08X", Build::GameImageSize());
    ImGui::TextUnformatted("Engine branch: DieselX (semantic public version not established)");

    if (BeginPointerTable("engine_pointers")) {
        PointerRow("Application", application, sizeof(void*));
        PointerRow("Context", context, sizeof(void*));
        PointerRow("World", world, sizeof(void*));
        PointerRow("UnitManager", unitManager, sizeof(void*));
        PointerRow("UnitManager owner", unitManagerOwner, sizeof(void*));
        PointerRow("Input", input, sizeof(void*));
        PointerRow("LuaInterpreter", luaOwner, sizeof(void*));
        PointerRow("VirtualFileSystem", vfs, sizeof(void*));
        PointerRow("MainDatabase", database, sizeof(void*));
        PointerRow("RenderInterface", renderInterface, sizeof(void*));
        PointerRow("D3DRenderDevice", renderDevice, sizeof(void*));
        PointerRow("IDirect3DDevice9", d3d, sizeof(void*));
        ImGui::EndTable();
    }

    Diesel::Application applicationCopy = {};
    if (application != NULL && Memory::TryRead(application, applicationCopy)) {
        ImGui::SeparatorText("Application fields");
        ImGui::Text("World member: 0x%08X", static_cast<unsigned int>(
            reinterpret_cast<std::uintptr_t>(applicationCopy.world)));
        ImGui::Text("Lua owner: 0x%08X", static_cast<unsigned int>(
            reinterpret_cast<std::uintptr_t>(applicationCopy.lua)));
        ImGui::Text("Render interface: 0x%08X", static_cast<unsigned int>(
            reinterpret_cast<std::uintptr_t>(applicationCopy.renderInterface)));
    }

    Diesel::Context contextCopy = {};
    if (context != NULL && Memory::TryRead(context, contextCopy)) {
        ImGui::SeparatorText("Context flags");
        ImGui::Text("Nil render device: %s", contextCopy.useNilRenderDevice ? "true" : "false");
        ImGui::Text("Threaded renderer: %s", contextCopy.threadedRenderer ? "true" : "false");
    }

    if (unitManager != NULL) {
        unsigned int readableSlots = 0;
        for (std::size_t index = 0; index < 64; ++index) {
            if (Diesel::Game::GetUnitSlot(unitManager, index) != NULL) {
                ++readableSlots;
            }
        }
        ImGui::SeparatorText("Unit manager fields");
        ImGui::Text("Confirmed slots: 64; populated readable pointers: %u",
                    readableSlots);
    }

    const std::vector<Diesel::Lua::StateInfo> states = Diesel::Lua::EnumerateStates();
    ImGui::Text("Published Lua state count: %u", static_cast<unsigned int>(states.size()));
    ImGui::Text("Game HWND: 0x%08X", static_cast<unsigned int>(
        reinterpret_cast<std::uintptr_t>(Diesel::Renderer::GetWindow())));
    ImGui::TextDisabled(
        "Confirmed Director/CameraController/WorldViewPort layouts are public, but no unique "
        "runtime root for the active viewport or player has been established.");
}

void DrawHooksTab() {
    const std::vector<Hooks::Status> hooks = Hooks::Snapshot();
    if (ImGui::BeginTable("hooks", 4,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                          ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Hook");
        ImGui::TableSetupColumn("Target");
        ImGui::TableSetupColumn("State");
        ImGui::TableSetupColumn("Diagnostic");
        ImGui::TableHeadersRow();
        for (std::size_t i = 0; i < hooks.size(); ++i) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(hooks[i].name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("0x%08X", static_cast<unsigned int>(hooks[i].target));
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(hooks[i].enabled ? "enabled" :
                                   (hooks[i].created ? "created" : "unavailable"));
            ImGui::TableSetColumnIndex(3);
            ImGui::TextWrapped("%s", hooks[i].error.c_str());
        }
        ImGui::EndTable();
    }
}

void DrawMemoryTab() {
    ImGui::TextWrapped(
        "This page only checks known symbol-derived roots. ScreamSDK does not scan or "
        "dereference arbitrary process addresses.");
    if (BeginPointerTable("memory_roots")) {
        const std::vector<Diesel::Lua::StateInfo> states = Diesel::Lua::EnumerateStates();
        PointerRow("game image", reinterpret_cast<const void*>(Build::GameBase()), 0x1000);
        PointerRow("Application", Diesel::Core::GetApplication(), sizeof(void*));
        PointerRow("World", Diesel::Core::GetWorld(), sizeof(void*));
        PointerRow("Lua main snapshot",
                   states.empty() ? NULL : states[0].state,
                   sizeof(void*));
        PointerRow("D3D9 device", Diesel::Renderer::GetD3D9Device(), sizeof(void*));
        ImGui::EndTable();
    }
}

void DrawLogTab() {
    const std::vector<std::string> lines = Log::Snapshot();
    if (ImGui::Button("Copy all")) {
        std::string text;
        for (std::size_t i = 0; i < lines.size(); ++i) {
            text += lines[i];
        }
        ImGui::SetClipboardText(text.c_str());
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%u buffered lines", static_cast<unsigned int>(lines.size()));
    if (ImGui::BeginChild("log_lines", ImVec2(0, 0), true,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(lines.size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                ImGui::TextUnformatted(lines[i].c_str());
            }
        }
    }
    ImGui::EndChild();
}

void DrawAboutTab() {
    ImGui::TextUnformatted("ScreamSDK / Lua Workbench 0.3.0-RELEASE");
    ImGui::Text("Dear ImGui %s, Win32 + Direct3D 9 backends", IMGUI_VERSION);
    ImGui::TextUnformatted("Target: Scream public development prototype, Win32/x86");
    ImGui::Text("Expected PE timestamp: 0x%08X", Build::kScreamPublicDev.timeDateStamp);
    ImGui::Text("Expected SizeOfImage: 0x%08X", Build::kScreamPublicDev.imageSize);
    ImGui::TextWrapped("The Lua executor uses the game's statically linked Lua 5.1.1-derived "
                       "runtime. It does not embed a second interpreter.");
    ImGui::TextWrapped("Loaded-script editing recompiles and re-executes a chunk. Existing "
                       "closures are not silently patched, and script side effects remain the "
                       "mod author's responsibility.");
    const char* keyNames[] = { "INSERT", "F1", "F2", "F10" };
    const int keyValues[] = { VK_INSERT, VK_F1, VK_F2, VK_F10 };
    int selectedKey = 0;
    for (int i = 0; i < 4; ++i) {
        if (Overlay::ToggleKey() == keyValues[i]) {
            selectedKey = i;
        }
    }
    if (ImGui::Combo("Overlay toggle key", &selectedKey, keyNames, 4)) {
        Overlay::SetToggleKey(keyValues[selectedKey]);
    }
    ImGui::Separator();
    ImGui::TextDisabled("The configured key toggles the overlay. Shutdown restores the window "
                        "procedure and removes every installed hook.");
    if (ImGui::Button("Request clean shutdown")) {
        Runtime::RequestShutdown();
    }
}

} // namespace Workbench
} // namespace Scream
