#include "ScreamSDK/Plugins.hpp"

#include "Internal.hpp"
#include "ScreamSDK/Log.hpp"

#include <Windows.h>

#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Scream {
namespace PluginsInternal {

namespace {

struct Panel {
    std::uint64_t handle;
    std::string id;
    std::string title;
    std::string tabLabel;
    std::int32_t sortOrder;
    void* userData;
    ScreamSDK_OverlayDrawCallback draw;
    HMODULE owner;
    bool active;
    std::uint32_t activeCalls;
};

std::mutex g_mutex;
std::vector<std::shared_ptr<Panel> > g_panels;
std::uint64_t g_nextHandle = 1;
bool g_accepting = false;
std::atomic<bool> g_shutdownComplete(true);

bool IsBoundedText(const char* text, std::size_t capacity) {
    return text != NULL && std::find(text, text + capacity, '\0') != text + capacity;
}

void DisableAfterFailure(const std::shared_ptr<Panel>& panel,
                         const char* diagnostic) {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        panel->active = false;
    }
    SCREAM_LOG_ERROR("Overlay plugin '%s' was disabled: %s",
                     panel->id.c_str(), diagnostic);
}

} // namespace

void Initialize() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_accepting = true;
    g_shutdownComplete.store(false);
}

std::uint64_t Register(const ScreamSDK_OverlayPanelV1* descriptor) {
    if (descriptor == NULL ||
        descriptor->structSize < sizeof(ScreamSDK_OverlayPanelV1) ||
        descriptor->abiVersion != SCREAMSDK_OVERLAY_PANEL_ABI_V1 ||
        descriptor->draw == NULL ||
        !IsBoundedText(descriptor->id, 128) ||
        !IsBoundedText(descriptor->title, 128) ||
        descriptor->id[0] == '\0' || descriptor->title[0] == '\0') {
        return 0;
    }

    HMODULE owner = NULL;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCSTR>(descriptor->draw), &owner)) {
        return 0;
    }

    std::shared_ptr<Panel> panel;
    try {
        panel.reset(new Panel());
        panel->id = descriptor->id;
        panel->title = descriptor->title;
        panel->tabLabel = panel->title + "###ScreamSDK.Plugin." + panel->id;
    } catch (...) {
        FreeLibrary(owner);
        return 0;
    }
    panel->sortOrder = descriptor->sortOrder;
    panel->userData = descriptor->userData;
    panel->draw = descriptor->draw;
    panel->owner = owner;
    panel->active = true;
    panel->activeCalls = 0;

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_accepting) {
            FreeLibrary(owner);
            return 0;
        }
        for (std::size_t i = 0; i < g_panels.size(); ++i) {
            if (g_panels[i]->active && g_panels[i]->id == panel->id) {
                FreeLibrary(owner);
                return 0;
            }
        }
        panel->handle = g_nextHandle++;
        if (panel->handle == 0) {
            panel->handle = g_nextHandle++;
        }
        g_panels.push_back(panel);
    }

    SCREAM_LOG_INFO("Registered overlay plugin '%s' (%s)",
                    panel->id.c_str(), panel->title.c_str());
    return panel->handle;
}

bool Unregister(std::uint64_t handle) {
    if (handle == 0) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    for (std::size_t i = 0; i < g_panels.size(); ++i) {
        if (g_panels[i]->handle == handle && g_panels[i]->active) {
            g_panels[i]->active = false;
            return true;
        }
    }
    return false;
}

bool IsRegistered(std::uint64_t handle) {
    if (handle == 0) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    for (std::size_t i = 0; i < g_panels.size(); ++i) {
        if (g_panels[i]->handle == handle) {
            return g_panels[i]->active;
        }
    }
    return false;
}

void DrawOverlayPanels() {
    std::vector<std::shared_ptr<Panel> > panels;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        for (std::size_t i = 0; i < g_panels.size(); ++i) {
            if (g_panels[i]->active) {
                panels.push_back(g_panels[i]);
            }
        }
    }
    std::sort(panels.begin(), panels.end(),
              [](const std::shared_ptr<Panel>& left,
                 const std::shared_ptr<Panel>& right) {
                  if (left->sortOrder != right->sortOrder) {
                      return left->sortOrder < right->sortOrder;
                  }
                  return left->id < right->id;
              });

    for (std::size_t i = 0; i < panels.size(); ++i) {
        const std::shared_ptr<Panel>& panel = panels[i];
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (!panel->active) {
                continue;
            }
            ++panel->activeCalls;
        }

        bool tabOpen = false;
        try {
            tabOpen = ImGui::BeginTabItem(panel->tabLabel.c_str());
            if (tabOpen) {
                panel->draw(panel->userData);
            }
        } catch (const std::exception& error) {
            DisableAfterFailure(panel, error.what());
        } catch (...) {
            DisableAfterFailure(panel, "unhandled C++ exception");
        }
        if (tabOpen) {
            ImGui::EndTabItem();
        }

        {
            std::lock_guard<std::mutex> lock(g_mutex);
            --panel->activeCalls;
        }
    }
}

bool Shutdown() {
    std::vector<HMODULE> owners;
    bool safe = true;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_accepting = false;
        for (std::size_t i = 0; i < g_panels.size(); ++i) {
            g_panels[i]->active = false;
            if (g_panels[i]->activeCalls == 0) {
                owners.push_back(g_panels[i]->owner);
                g_panels[i]->owner = NULL;
            } else {
                safe = false;
            }
        }
        if (safe) {
            g_panels.clear();
        }
    }
    for (std::size_t i = 0; i < owners.size(); ++i) {
        if (owners[i] != NULL) {
            FreeLibrary(owners[i]);
        }
    }
    g_shutdownComplete.store(safe);
    return safe;
}

bool IsShutdownComplete() {
    return g_shutdownComplete.load();
}

} // namespace PluginsInternal
} // namespace Scream

extern "C" std::uint64_t SCREAM_CDECL ScreamSDK_RegisterOverlayPanel(
    const ScreamSDK_OverlayPanelV1* panel) {
    return Scream::PluginsInternal::Register(panel);
}

extern "C" bool SCREAM_CDECL ScreamSDK_UnregisterOverlayPanel(
    std::uint64_t handle) {
    return Scream::PluginsInternal::Unregister(handle);
}

extern "C" bool SCREAM_CDECL ScreamSDK_IsOverlayPanelRegistered(
    std::uint64_t handle) {
    return Scream::PluginsInternal::IsRegistered(handle);
}
