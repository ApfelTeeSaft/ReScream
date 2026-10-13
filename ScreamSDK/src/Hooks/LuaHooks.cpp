#include "Internal.hpp"

#include "Diesel/Lua.hpp"
#include "ScreamSDK/Addresses.hpp"
#include "ScreamSDK/Build.hpp"
#include "ScreamSDK/Log.hpp"

#include <mutex>

namespace Scream {
namespace LuaHooks {

namespace {

typedef const char* (SCREAM_CDECL* LuaReader)(lua_State*, void*, unsigned int*);
typedef int (SCREAM_CDECL* LuaLoadBuffer)(lua_State*, const char*, unsigned int,
                                         const char*);
typedef int (SCREAM_CDECL* LuaLoad)(lua_State*, LuaReader, void*, const char*, int);

LuaLoadBuffer g_originalLoadBuffer = NULL;
LuaLoad g_originalLoad = NULL;
thread_local unsigned int g_insideLoadBuffer = 0;
std::recursive_mutex g_callbackMutex;

int SCREAM_CDECL LoadBufferDetour(lua_State* state,
                                  const char* buffer,
                                  unsigned int size,
                                  const char* name) {
    std::lock_guard<std::recursive_mutex> callbackLock(g_callbackMutex);
    ++g_insideLoadBuffer;
    const int status = g_originalLoadBuffer(state, buffer, size, name);
    --g_insideLoadBuffer;
    if (status == 0 && !Runtime::IsShuttingDown()) {
        try {
            Diesel::Lua::RecordBufferLoad(state, name, buffer, size);
        } catch (...) {
            SCREAM_LOG_WARN("Could not retain luaL_loadbuffer metadata");
        }
    }
    return status;
}

int SCREAM_CDECL LoadDetour(lua_State* state,
                            LuaReader reader,
                            void* data,
                            const char* name,
                            int startLine) {
    std::lock_guard<std::recursive_mutex> callbackLock(g_callbackMutex);
    const int status = g_originalLoad(state, reader, data, name, startLine);
    if (status == 0 && g_insideLoadBuffer == 0 && !Runtime::IsShuttingDown()) {
        try {
            Diesel::Lua::RecordStreamLoad(state, name);
        } catch (...) {
            SCREAM_LOG_WARN("Could not retain lua_load metadata");
        }
    }
    return status;
}

} // namespace

bool Install() {
    const bool buffer = InternalHooks::Create(
        "luaL_loadbuffer", Build::ResolveRva<void*>(Addresses::LuaLLoadBuffer),
        reinterpret_cast<void*>(&LoadBufferDetour),
        reinterpret_cast<void**>(&g_originalLoadBuffer));
    const bool stream = InternalHooks::Create(
        "lua_load", Build::ResolveRva<void*>(Addresses::LuaLoad),
        reinterpret_cast<void*>(&LoadDetour),
        reinterpret_cast<void**>(&g_originalLoad));
    return buffer && stream;
}

void Quiesce() {
    std::lock_guard<std::recursive_mutex> callbackLock(g_callbackMutex);
}

} // namespace LuaHooks
} // namespace Scream
