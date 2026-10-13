#pragma once

#include "ScreamSDK/Platform.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct lua_State;

namespace Diesel {

#pragma pack(push, 1)
struct LuaInterpreter {
    lua_State* state;                     // +0x00 LuaInterface::L
    LuaInterpreter* next;                 // +0x04 InstanceList
    LuaInterpreter* previous;             // +0x08 InstanceList
    std::int32_t classesReference;        // +0x0C
    std::int32_t version;                 // +0x10 (non-authoritative storage)
    bool active;                          // +0x14
    bool classes;                         // +0x15
    std::uint8_t padding_16[2];
    std::int32_t thread;                  // +0x18
    std::uint8_t unrefQueueLock[0x18];    // +0x1C CriticalSection
    std::uint8_t unrefQueue[0x18];        // +0x34 MSVC9 checked vector
};

struct LuaInterpreterRecord {
    LuaInterpreter* interpreter;
    lua_State* state;
    std::int32_t version;
};
#pragma pack(pop)

static_assert(offsetof(LuaInterpreter, state) == 0x00, "LuaInterface::L");
static_assert(offsetof(LuaInterpreter, active) == 0x14,
              "LuaInterpreter::_active");
static_assert(offsetof(LuaInterpreter, thread) == 0x18,
              "LuaInterpreter::_thread");
static_assert(sizeof(LuaInterpreter) == 0x4C, "LuaInterpreter size");
static_assert(sizeof(LuaInterpreterRecord) == 0x0C,
              "LuaInterpreter::Interpreter size");

namespace Lua {

struct StateInfo {
    LuaInterpreter* owner;
    lua_State* state;
    std::int32_t version;
    std::int32_t thread;
    bool main;
    bool active;
};

struct LoadedScript {
    lua_State* state;
    std::string chunkName;
    std::uint32_t size;
    std::uint32_t loadCount;
    bool sourceAvailable;
    std::string source;
};

struct ScriptResource {
    std::string logicalPath;
    std::string containerPath;
    std::uint64_t archiveOffset;
    std::uint32_t size;
    bool compiled;
    bool sourceAvailable;
};

struct ExecutionResult {
    std::uint64_t requestId;
    bool complete;
    bool success;
    int luaStatus;
    std::string chunkName;
    std::string error;
    std::vector<std::string> output;
    std::vector<std::string> returnValues;
};

struct EnvironmentEntry {
    std::string path;
    std::string key;
    std::string type;
    std::string value;
    std::uintptr_t address;
    std::uint32_t depth;
    bool table;
    bool cycle;
    bool childrenOmitted;
};

struct EnvironmentSnapshot {
    std::uint64_t requestId;
    bool complete;
    bool success;
    bool truncated;
    std::string error;
    std::vector<EnvironmentEntry> entries;
};

SCREAM_API void Initialize();
SCREAM_API void Shutdown();
SCREAM_API void OnMainThreadUpdate();
SCREAM_API std::vector<StateInfo> EnumerateStates();
SCREAM_API std::vector<LoadedScript> LoadedScripts();
SCREAM_API std::vector<ScriptResource> CatalogScripts();
SCREAM_API bool LoadCatalogSource(const ScriptResource& resource,
                                  std::string& source,
                                  std::string& error);
SCREAM_API std::uint64_t QueueExecute(const StateInfo& state,
                                     const std::string& source,
                                     const std::string& chunkName);
SCREAM_API std::uint64_t QueueVfsLoad(const StateInfo& state,
                                     const std::string& logicalPath);
SCREAM_API bool TryGetResult(std::uint64_t requestId, ExecutionResult& result);
SCREAM_API std::uint64_t QueueEnvironmentSnapshot(const StateInfo& state,
                                                  std::uint32_t maximumDepth = 3,
                                                  std::uint32_t maximumItems = 512);
SCREAM_API bool TryGetEnvironmentSnapshot(std::uint64_t requestId,
                                          EnvironmentSnapshot& snapshot);
SCREAM_API void RecordBufferLoad(lua_State* state,
                                const char* chunkName,
                                const char* source,
                                std::uint32_t size);
SCREAM_API void RecordStreamLoad(lua_State* state, const char* chunkName);

} // namespace Lua
} // namespace Diesel
