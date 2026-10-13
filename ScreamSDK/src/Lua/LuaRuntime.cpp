#include "Diesel/Lua.hpp"

#include "Diesel/Core.hpp"
#include "Diesel/Game.hpp"
#include "Internal.hpp"
#include "ScreamSDK/Addresses.hpp"
#include "ScreamSDK/Build.hpp"
#include "ScreamSDK/Log.hpp"
#include "ScreamSDK/Memory.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <deque>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <sstream>

namespace Diesel {
namespace Lua {

namespace {

const int kLuaMultipleReturns = -1;
const int kLuaRegistryIndex = -10000;
const int kLuaGlobalsIndex = -10002;
const std::size_t kMaximumSourceBytes = 2u * 1024u * 1024u;
const std::size_t kMaximumLoadedScripts = 2048;
const std::size_t kMaximumCapturedSourceBytes = 32u * 1024u * 1024u;
const std::size_t kMaximumRequests = 64;
const std::size_t kMaximumSnapshotRequests = 8;
const std::uint32_t kMaximumSnapshotDepth = 6;
const std::uint32_t kMaximumSnapshotItems = 4096;

typedef int (SCREAM_CDECL* LuaGetTop)(lua_State*);
typedef void (SCREAM_CDECL* LuaSetTop)(lua_State*, int);
typedef void (SCREAM_CDECL* LuaRemove)(lua_State*, int);
typedef int (SCREAM_CDECL* LuaType)(lua_State*, int);
typedef const char* (SCREAM_CDECL* LuaTypeName)(lua_State*, int);
typedef int (SCREAM_CDECL* LuaToBoolean)(lua_State*, int);
typedef double (SCREAM_CDECL* LuaToNumber)(lua_State*, int);
typedef int (SCREAM_CDECL* LuaToInteger)(lua_State*, int);
typedef const void* (SCREAM_CDECL* LuaToPointer)(lua_State*, int);
typedef const char* (SCREAM_CDECL* LuaToString)(lua_State*, int, unsigned int*);
typedef void (SCREAM_CDECL* LuaPushValue)(lua_State*, int);
typedef void (SCREAM_CDECL* LuaPushNil)(lua_State*);
typedef void (SCREAM_CDECL* LuaPushString)(lua_State*, const char*);
typedef void (SCREAM_CDECL* LuaRawGet)(lua_State*, int);
typedef void (SCREAM_CDECL* LuaRawGetI)(lua_State*, int, int);
typedef void (SCREAM_CDECL* LuaRawSet)(lua_State*, int);
typedef int (SCREAM_CDECL* LuaNext)(lua_State*, int);
typedef int (SCREAM_CDECL* LuaCFunction)(lua_State*);
typedef void (SCREAM_CDECL* LuaPushCClosure)(lua_State*, LuaCFunction, int);
typedef int (SCREAM_CDECL* LuaLoadBuffer)(lua_State*, const char*, unsigned int,
                                         const char*);
typedef int (SCREAM_CDECL* LuaPCall)(lua_State*, int, int, int);
typedef LuaInterpreter* (SCREAM_CDECL* InterpreterFor)(lua_State*, int);

struct Api {
    LuaGetTop getTop;
    LuaSetTop setTop;
    LuaRemove remove;
    LuaType type;
    LuaTypeName typeName;
    LuaToBoolean toBoolean;
    LuaToNumber toNumber;
    LuaToInteger toInteger;
    LuaToPointer toPointer;
    LuaToString toString;
    LuaPushValue pushValue;
    LuaPushNil pushNil;
    LuaPushString pushString;
    LuaRawGet rawGet;
    LuaRawGetI rawGetI;
    LuaRawSet rawSet;
    LuaNext next;
    LuaPushCClosure pushCClosure;
    LuaLoadBuffer loadBuffer;
    LuaPCall protectedCall;
    InterpreterFor interpreterFor;
};

struct Request {
    std::uint64_t id;
    StateInfo state;
    std::string source;
    std::string chunkName;
};

struct SnapshotRequest {
    std::uint64_t id;
    StateInfo state;
    std::uint32_t maximumDepth;
    std::uint32_t maximumItems;
};

struct Capture {
    Api* api;
    std::vector<std::string>* output;
};

Api g_api = {};
std::mutex g_stateMutex;
std::vector<StateInfo> g_states;
std::mutex g_loadedMutex;
std::vector<LoadedScript> g_loadedScripts;
std::mutex g_requestMutex;
std::deque<Request> g_requests;
std::map<std::uint64_t, ExecutionResult> g_results;
std::uint64_t g_nextRequestId = 1;
std::mutex g_snapshotMutex;
std::deque<SnapshotRequest> g_snapshotRequests;
std::map<std::uint64_t, EnvironmentSnapshot> g_snapshots;
std::uint64_t g_nextSnapshotRequestId = 1;
std::size_t g_lastLoggedStateCount = std::numeric_limits<std::size_t>::max();
bool g_loggedUninitializedOwnerVersion = false;
thread_local Capture* g_capture = NULL;

template <typename T>
T Resolve(std::uintptr_t rva) {
    return Scream::Build::ResolveRva<T>(rva);
}

bool ResolveApi() {
    g_api.getTop = Resolve<LuaGetTop>(Scream::Addresses::LuaGetTop);
    g_api.setTop = Resolve<LuaSetTop>(Scream::Addresses::LuaSetTop);
    g_api.remove = Resolve<LuaRemove>(Scream::Addresses::LuaRemove);
    g_api.type = Resolve<LuaType>(Scream::Addresses::LuaType);
    g_api.typeName = Resolve<LuaTypeName>(Scream::Addresses::LuaTypeName);
    g_api.toBoolean = Resolve<LuaToBoolean>(Scream::Addresses::LuaToBoolean);
    g_api.toNumber = Resolve<LuaToNumber>(Scream::Addresses::LuaToNumber);
    g_api.toInteger = Resolve<LuaToInteger>(Scream::Addresses::LuaToInteger);
    g_api.toPointer = Resolve<LuaToPointer>(Scream::Addresses::LuaToPointer);
    g_api.toString = Resolve<LuaToString>(Scream::Addresses::LuaToString);
    g_api.pushValue = Resolve<LuaPushValue>(Scream::Addresses::LuaPushValue);
    g_api.pushNil = Resolve<LuaPushNil>(Scream::Addresses::LuaPushNil);
    g_api.pushString = Resolve<LuaPushString>(Scream::Addresses::LuaPushString);
    g_api.rawGet = Resolve<LuaRawGet>(Scream::Addresses::LuaRawGet);
    g_api.rawGetI = Resolve<LuaRawGetI>(Scream::Addresses::LuaRawGetI);
    g_api.rawSet = Resolve<LuaRawSet>(Scream::Addresses::LuaRawSet);
    g_api.next = Resolve<LuaNext>(Scream::Addresses::LuaNext);
    g_api.pushCClosure = Resolve<LuaPushCClosure>(Scream::Addresses::LuaPushCClosure);
    g_api.loadBuffer = Resolve<LuaLoadBuffer>(Scream::Addresses::LuaLLoadBuffer);
    g_api.protectedCall = Resolve<LuaPCall>(Scream::Addresses::LuaPCall);
    g_api.interpreterFor = Resolve<InterpreterFor>(Scream::Addresses::LuaInterpreterFor);

    const void* functions[] = {
        reinterpret_cast<const void*>(g_api.getTop),
        reinterpret_cast<const void*>(g_api.setTop),
        reinterpret_cast<const void*>(g_api.remove),
        reinterpret_cast<const void*>(g_api.type),
        reinterpret_cast<const void*>(g_api.typeName),
        reinterpret_cast<const void*>(g_api.toBoolean),
        reinterpret_cast<const void*>(g_api.toNumber),
        reinterpret_cast<const void*>(g_api.toInteger),
        reinterpret_cast<const void*>(g_api.toPointer),
        reinterpret_cast<const void*>(g_api.toString),
        reinterpret_cast<const void*>(g_api.pushValue),
        reinterpret_cast<const void*>(g_api.pushNil),
        reinterpret_cast<const void*>(g_api.pushString),
        reinterpret_cast<const void*>(g_api.rawGet),
        reinterpret_cast<const void*>(g_api.rawGetI),
        reinterpret_cast<const void*>(g_api.rawSet),
        reinterpret_cast<const void*>(g_api.next),
        reinterpret_cast<const void*>(g_api.pushCClosure),
        reinterpret_cast<const void*>(g_api.loadBuffer),
        reinterpret_cast<const void*>(g_api.protectedCall),
        reinterpret_cast<const void*>(g_api.interpreterFor)
    };
    for (std::size_t i = 0; i < sizeof(functions) / sizeof(functions[0]); ++i) {
        if (!Scream::Memory::IsExecutable(functions[i]) ||
            !Scream::Memory::IsAddressInGame(functions[i])) {
            return false;
        }
    }
    return true;
}

std::string ReadCString(const char* value, std::size_t maximum) {
    if (value == NULL) {
        return std::string("?");
    }
    std::string result;
    result.reserve(std::min<std::size_t>(maximum, 256));
    for (std::size_t i = 0; i < maximum; ++i) {
        char character = 0;
        if (!Scream::Memory::TryRead(value + i, character)) {
            return result.empty() ? std::string("<unreadable>") : result;
        }
        if (character == '\0') {
            return result;
        }
        result.push_back(character);
    }
    result += "...";
    return result;
}

std::string ValueToString(lua_State* state, int index, Api& api) {
    const int type = api.type(state, index);
    if (type == 0) {
        return "nil";
    }
    if (type == 1) {
        return api.toBoolean(state, index) ? "true" : "false";
    }
    if (type == 3) {
        std::ostringstream result;
        result << api.toNumber(state, index);
        return result.str();
    }
    if (type == 4) {
        unsigned int size = 0;
        const char* text = api.toString(state, index, &size);
        if (text == NULL) {
            return "<conversion returned null>";
        }
        const std::size_t kept = std::min<std::size_t>(size, 4096);
        std::string result(text, text + kept);
        if (kept != size) {
            result += "...";
        }
        return result;
    }

    const char* name = api.typeName(state, type);
    std::ostringstream result;
    result << (name != NULL ? name : "unknown");
    const void* pointer = api.toPointer(state, index);
    if (pointer != NULL) {
        result << "@0x" << std::hex << std::uppercase
               << reinterpret_cast<std::uintptr_t>(pointer);
    }
    return result.str();
}

int SCREAM_CDECL CapturePrint(lua_State* state) {
    Capture* capture = g_capture;
    if (capture == NULL || capture->api == NULL || capture->output == NULL) {
        return 0;
    }
    try {
        std::ostringstream line;
        const int count = capture->api->getTop(state);
        for (int i = 1; i <= count; ++i) {
            if (i != 1) {
                line << '\t';
            }
            line << ValueToString(state, i, *capture->api);
        }
        if (capture->output->size() < 512) {
            capture->output->push_back(line.str());
        }
    } catch (...) {
        // Never allow a C++ exception to cross Lua's C ABI.
    }
    return 0;
}

bool SameState(const StateInfo& left, const StateInfo& right) {
    return left.owner == right.owner && left.state == right.state &&
           left.version == right.version;
}

void EnforceSourceBudget() {
    std::size_t total = 0;
    for (std::size_t i = 0; i < g_loadedScripts.size(); ++i) {
        total += g_loadedScripts[i].source.size();
    }
    for (std::size_t i = 0;
         i < g_loadedScripts.size() && total > kMaximumCapturedSourceBytes; ++i) {
        if (g_loadedScripts[i].source.empty()) {
            continue;
        }
        total -= g_loadedScripts[i].source.size();
        g_loadedScripts[i].source.clear();
        g_loadedScripts[i].sourceAvailable = false;
    }
}

bool IsCurrentlyValid(const StateInfo& requested,
                      const std::vector<StateInfo>& current) {
    if (!requested.main || !requested.active) {
        return false;
    }
    for (std::size_t i = 0; i < current.size(); ++i) {
        if (SameState(requested, current[i]) && current[i].active && current[i].main) {
            return true;
        }
    }
    return false;
}

LuaInterpreter* MapInterpreter(lua_State* state, int version) {
#if defined(_MSC_VER)
    __try {
        return g_api.interpreterFor(state, version);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return NULL;
    }
#else
    return g_api.interpreterFor(state, version);
#endif
}

bool ReadRegistryVersion(lua_State* state, int& version) {
    // newstate stores the monotonically increasing interpreter version at
    // registry reference 2. LuaRef::set and LuaInterpreter's own registry use
    // this value; the PDB-named owner member at +0x10 is never initialized by
    // the analyzed constructor and contains stale allocation bytes in practice.
#if defined(_MSC_VER)
    __try {
#endif
        const int top = g_api.getTop(state);
        g_api.rawGetI(state, kLuaRegistryIndex, 2);
        version = g_api.toInteger(state, -1);
        g_api.setTop(state, top);
        return version > 0;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#endif
}

std::vector<StateInfo> WalkStates() {
    std::vector<StateInfo> result;
    LuaInterpreter* first = NULL;
    LuaInterpreter* main = NULL;
    const std::uintptr_t base = Scream::Build::GameBase();
    Scream::Memory::TryRead(reinterpret_cast<const void*>(
        base + Scream::Addresses::LuaInterpreterFirst), first);
    Scream::Memory::TryRead(reinterpret_cast<const void*>(
        base + Scream::Addresses::LuaInterpreterMain), main);

    std::set<LuaInterpreter*> visited;
    LuaInterpreter* cursor = first;
    for (std::size_t count = 0; cursor != NULL && count < 128; ++count) {
        if (!visited.insert(cursor).second ||
            !Scream::Memory::IsReadable(cursor, sizeof(LuaInterpreter))) {
            break;
        }
        LuaInterpreter copy = {};
        if (!Scream::Memory::TryRead(cursor, copy)) {
            break;
        }
        if (copy.state != NULL && copy.active &&
            Scream::Memory::IsReadable(copy.state, sizeof(void*))) {
            int version = 0;
            if (!ReadRegistryVersion(copy.state, version)) {
                cursor = copy.next;
                continue;
            }
            if (copy.version != version && !g_loggedUninitializedOwnerVersion) {
                g_loggedUninitializedOwnerVersion = true;
                SCREAM_LOG_DEBUG(
                    "LuaInterpreter::_version is non-authoritative (owner bytes=0x%08X, registry=%d)",
                    static_cast<unsigned int>(copy.version), version);
            }
            LuaInterpreter* mapped = MapInterpreter(copy.state, version);
            if (mapped == cursor) {
                StateInfo info = {};
                info.owner = cursor;
                info.state = copy.state;
                info.version = version;
                info.thread = copy.thread;
                info.main = cursor == main;
                info.active = copy.active;
                result.push_back(info);
            }
        }
        cursor = copy.next;
    }

    // The PDB-confirmed Application member is a useful startup/teardown cross-check.
    Application* application = Core::GetApplication();
    if (application != NULL &&
        Scream::Memory::IsReadable(application,
            offsetof(Application, lua) + sizeof(application->lua))) {
        LuaInterpreter* owner = NULL;
        if (Scream::Memory::TryRead(&application->lua, owner) && owner != NULL &&
            visited.find(owner) == visited.end() &&
            Scream::Memory::IsReadable(owner, sizeof(LuaInterpreter))) {
            LuaInterpreter copy = {};
            int version = 0;
            if (Scream::Memory::TryRead(owner, copy) && copy.active &&
                copy.state != NULL && ReadRegistryVersion(copy.state, version) &&
                MapInterpreter(copy.state, version) == owner) {
                StateInfo info = {};
                info.owner = owner;
                info.state = copy.state;
                info.version = version;
                info.thread = copy.thread;
                info.main = owner == main;
                info.active = true;
                result.push_back(info);
            }
        }
    }
    return result;
}

ExecutionResult Execute(const Request& request) {
    ExecutionResult result = {};
    result.requestId = request.id;
    result.complete = true;
    result.success = false;
    result.luaStatus = -1;
    result.chunkName = request.chunkName;

    lua_State* state = request.state.state;
    const int originalTop = g_api.getTop(state);
    Capture capture = { &g_api, &result.output };
    Capture* previousCapture = g_capture;
    bool savedPrint = false;

    try {
        // Use raw global-table access: this cannot invoke a hostile _G
        // __index/__newindex metamethod outside lua_pcall. The saved value stays
        // below the compiled chunk/results until restoration.
        g_api.pushString(state, "print");
        g_api.rawGet(state, kLuaGlobalsIndex);
        savedPrint = true;
        g_api.pushString(state, "print");
        g_api.pushCClosure(state, &CapturePrint, 0);
        g_api.rawSet(state, kLuaGlobalsIndex);

        const int loadStatus = g_api.loadBuffer(
            state, request.source.data(), static_cast<unsigned int>(request.source.size()),
            request.chunkName.c_str());
        int callStatus = loadStatus;
        if (loadStatus == 0) {
            g_capture = &capture;
            callStatus = g_api.protectedCall(state, 0, kLuaMultipleReturns, 0);
            g_capture = previousCapture;
        }

        g_api.pushString(state, "print");
        g_api.pushValue(state, originalTop + 1);
        g_api.rawSet(state, kLuaGlobalsIndex);
        g_api.remove(state, originalTop + 1);
        savedPrint = false;

        result.luaStatus = callStatus;
        if (callStatus != 0) {
            result.error = ValueToString(state, -1, g_api);
        } else {
            const int finalTop = g_api.getTop(state);
            const int returnCount = std::max(0, finalTop - originalTop);
            const int captured = std::min(returnCount, 32);
            for (int i = 0; i < captured; ++i) {
                result.returnValues.push_back(
                    ValueToString(state, originalTop + 1 + i, g_api));
            }
            if (captured != returnCount) {
                result.returnValues.push_back("<additional return values omitted>");
            }
            result.success = true;
        }
    } catch (...) {
        if (savedPrint) {
            g_api.pushString(state, "print");
            g_api.pushValue(state, originalTop + 1);
            g_api.rawSet(state, kLuaGlobalsIndex);
        }
        g_capture = previousCapture;
        g_api.setTop(state, originalTop);
        throw;
    }

    g_capture = previousCapture;
    g_api.setTop(state, originalTop);
    return result;
}

std::string EscapeLuaString(const std::string& input) {
    std::ostringstream result;
    result << '"';
    for (std::size_t i = 0; i < input.size(); ++i) {
        const unsigned char character = static_cast<unsigned char>(input[i]);
        switch (character) {
        case '\\': result << "\\\\"; break;
        case '"': result << "\\\""; break;
        case '\n': result << "\\n"; break;
        case '\r': result << "\\r"; break;
        case '\t': result << "\\t"; break;
        default:
            if (character < 32 || character == 127) {
                result << '\\' << std::setw(3) << std::setfill('0')
                       << static_cast<unsigned int>(character);
            } else {
                result << static_cast<char>(character);
            }
            break;
        }
    }
    result << '"';
    return result.str();
}

void StoreResult(const ExecutionResult& result) {
    std::lock_guard<std::mutex> lock(g_requestMutex);
    g_results[result.requestId] = result;
    while (g_results.size() > 256) {
        g_results.erase(g_results.begin());
    }
}

class LuaStackGuard {
public:
    LuaStackGuard(lua_State* state, Api& api)
        : state_(state), api_(api), top_(api.getTop(state)) {}

    ~LuaStackGuard() {
        api_.setTop(state_, top_);
    }

private:
    lua_State* state_;
    Api& api_;
    int top_;
};

std::string SnapshotText(lua_State* state, int index, Api& api) {
    std::string text = ValueToString(state, index, api);
    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char character = static_cast<unsigned char>(text[i]);
        if (character == '\r' || character == '\n' || character == '\t') {
            text[i] = ' ';
        } else if (character < 32 || character == 127) {
            text[i] = '?';
        }
    }
    if (text.size() > 256) {
        text.resize(256);
        text += "...";
    }
    return text;
}

struct SnapshotBuilder {
    lua_State* state;
    Api* api;
    EnvironmentSnapshot* snapshot;
    std::uint32_t maximumDepth;
    std::uint32_t maximumItems;
    std::set<const void*> activeTables;
};

std::string ChildPath(const std::string& parent, const std::string& key) {
    if (!key.empty() &&
        (std::isalpha(static_cast<unsigned char>(key[0])) || key[0] == '_')) {
        bool identifier = true;
        for (std::size_t i = 1; i < key.size(); ++i) {
            const unsigned char character = static_cast<unsigned char>(key[i]);
            if (!std::isalnum(character) && character != '_') {
                identifier = false;
                break;
            }
        }
        if (identifier) {
            return parent + "." + key;
        }
    }
    return parent + "[" + key + "]";
}

void SnapshotTable(SnapshotBuilder& builder, int tableIndex,
                   const std::string& path, std::uint32_t depth) {
    LuaStackGuard stack(builder.state, *builder.api);
    builder.api->pushNil(builder.state);
    while (builder.api->next(builder.state, tableIndex) != 0) {
        if (builder.snapshot->entries.size() >= builder.maximumItems) {
            builder.snapshot->truncated = true;
            return;
        }

        EnvironmentEntry entry = {};
        entry.key = SnapshotText(builder.state, -2, *builder.api);
        entry.path = ChildPath(path, entry.key);
        entry.depth = depth;
        const int valueType = builder.api->type(builder.state, -1);
        const char* typeName = builder.api->typeName(builder.state, valueType);
        entry.type = typeName != NULL ? typeName : "unknown";
        entry.value = SnapshotText(builder.state, -1, *builder.api);
        const void* pointer = builder.api->toPointer(builder.state, -1);
        entry.address = reinterpret_cast<std::uintptr_t>(pointer);
        entry.table = valueType == 5; // LUA_TTABLE in Lua 5.1.
        entry.cycle = entry.table && pointer != NULL &&
                      builder.activeTables.find(pointer) != builder.activeTables.end();
        entry.childrenOmitted = entry.table &&
            (entry.cycle || depth >= builder.maximumDepth);
        builder.snapshot->entries.push_back(entry);

        if (entry.table && !entry.childrenOmitted) {
            const int valueIndex = builder.api->getTop(builder.state);
            if (pointer != NULL) {
                builder.activeTables.insert(pointer);
            }
            SnapshotTable(builder, valueIndex, entry.path, depth + 1);
            if (pointer != NULL) {
                builder.activeTables.erase(pointer);
            }
        }

        // lua_next expects the key at the top for the following iteration.
        // Drop the value while retaining the key at -1.
        builder.api->setTop(builder.state, builder.api->getTop(builder.state) - 1);
    }
}

EnvironmentSnapshot BuildEnvironmentSnapshot(const SnapshotRequest& request) {
    EnvironmentSnapshot snapshot = {};
    snapshot.requestId = request.id;
    snapshot.complete = true;
    snapshot.success = false;

    LuaStackGuard stack(request.state.state, g_api);
    SnapshotBuilder builder = {};
    builder.state = request.state.state;
    builder.api = &g_api;
    builder.snapshot = &snapshot;
    builder.maximumDepth = request.maximumDepth;
    builder.maximumItems = request.maximumItems;
    const void* globals = g_api.toPointer(builder.state, kLuaGlobalsIndex);
    if (globals != NULL) {
        builder.activeTables.insert(globals);
    }
    SnapshotTable(builder, kLuaGlobalsIndex, "_G", 0);
    snapshot.success = true;
    return snapshot;
}

void StoreSnapshot(const EnvironmentSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(g_snapshotMutex);
    g_snapshots[snapshot.requestId] = snapshot;
    while (g_snapshots.size() > 32) {
        g_snapshots.erase(g_snapshots.begin());
    }
}

} // namespace

void Initialize() {
    if (!ResolveApi()) {
        SCREAM_LOG_ERROR("Lua ABI validation failed; execution is disabled");
        memset(&g_api, 0, sizeof(g_api));
        return;
    }
    Scream::LuaCatalogInternal::Discover();
    SCREAM_LOG_INFO("Lua 5.1.1 ABI resolved from the game module");
}

void Shutdown() {
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_states.clear();
    }
    {
        std::lock_guard<std::mutex> lock(g_loadedMutex);
        g_loadedScripts.clear();
    }
    {
        std::lock_guard<std::mutex> lock(g_requestMutex);
        g_requests.clear();
        g_results.clear();
    }
    {
        std::lock_guard<std::mutex> lock(g_snapshotMutex);
        g_snapshotRequests.clear();
        g_snapshots.clear();
    }
    Scream::LuaCatalogInternal::Clear();
    memset(&g_api, 0, sizeof(g_api));
    g_lastLoggedStateCount = std::numeric_limits<std::size_t>::max();
    g_loggedUninitializedOwnerVersion = false;
}

void OnMainThreadUpdate() {
    if (g_api.getTop == NULL || Scream::Runtime::IsShuttingDown()) {
        return;
    }
    const std::vector<StateInfo> current = WalkStates();
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_states = current;
    }
    if (current.size() != g_lastLoggedStateCount) {
        g_lastLoggedStateCount = current.size();
        SCREAM_LOG_INFO("Live Lua state count: %u", static_cast<unsigned int>(current.size()));
        for (std::size_t i = 0; i < current.size(); ++i) {
            SCREAM_LOG_INFO("  Lua state[%u]: owner=%p state=%p version=%d thread=%d main=%s",
                            static_cast<unsigned int>(i), current[i].owner, current[i].state,
                            current[i].version, current[i].thread,
                            current[i].main ? "yes" : "no");
        }
    }

    for (int processed = 0; processed < 4; ++processed) {
        Request request;
        {
            std::lock_guard<std::mutex> lock(g_requestMutex);
            if (g_requests.empty()) {
                break;
            }
            request = g_requests.front();
            g_requests.pop_front();
        }

        if (!IsCurrentlyValid(request.state, current)) {
            ExecutionResult failure = {};
            failure.requestId = request.id;
            failure.complete = true;
            failure.success = false;
            failure.luaStatus = -1;
            failure.chunkName = request.chunkName;
            failure.error = "selected Lua state is not the live Diesel _main interpreter";
            StoreResult(failure);
            continue;
        }

        try {
            ExecutionResult result = Execute(request);
            StoreResult(result);
            if (result.success) {
                SCREAM_LOG_INFO("Lua request %llu completed in state %p",
                                static_cast<unsigned long long>(request.id),
                                request.state.state);
            } else {
                SCREAM_LOG_WARN("Lua request %llu failed: %s",
                                static_cast<unsigned long long>(request.id),
                                result.error.c_str());
            }
        } catch (...) {
            if (g_api.setTop != NULL && request.state.state != NULL) {
                // The normal path restores the stack. This only covers C++ failures.
            }
            ExecutionResult failure = {};
            failure.requestId = request.id;
            failure.complete = true;
            failure.success = false;
            failure.luaStatus = -1;
            failure.chunkName = request.chunkName;
            failure.error = "unexpected C++ exception while executing Lua";
            StoreResult(failure);
        }
    }

    SnapshotRequest snapshotRequest = {};
    bool haveSnapshotRequest = false;
    {
        std::lock_guard<std::mutex> lock(g_snapshotMutex);
        if (!g_snapshotRequests.empty()) {
            snapshotRequest = g_snapshotRequests.front();
            g_snapshotRequests.pop_front();
            haveSnapshotRequest = true;
        }
    }
    if (haveSnapshotRequest) {
        EnvironmentSnapshot snapshot = {};
        snapshot.requestId = snapshotRequest.id;
        snapshot.complete = true;
        if (!IsCurrentlyValid(snapshotRequest.state, current)) {
            snapshot.error = "selected Lua state is not the live Diesel _main interpreter";
        } else {
            try {
                snapshot = BuildEnvironmentSnapshot(snapshotRequest);
            } catch (...) {
                snapshot.error = "unexpected C++ exception while snapshotting the Lua environment";
            }
        }
        StoreSnapshot(snapshot);
    }
}

std::vector<StateInfo> EnumerateStates() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return g_states;
}

std::vector<LoadedScript> LoadedScripts() {
    std::lock_guard<std::mutex> lock(g_loadedMutex);
    return g_loadedScripts;
}

std::uint64_t QueueExecute(const StateInfo& state,
                           const std::string& source,
                           const std::string& chunkName) {
    Request request;
    {
        std::lock_guard<std::mutex> lock(g_requestMutex);
        request.id = g_nextRequestId++;
        request.state = state;
        request.source = source;
        request.chunkName = chunkName.empty() ? "@ScreamSDK/Workbench" : chunkName;
        if (!state.main || !state.active || source.size() > kMaximumSourceBytes ||
            g_requests.size() >= kMaximumRequests) {
            ExecutionResult failure = {};
            failure.requestId = request.id;
            failure.complete = true;
            failure.success = false;
            failure.luaStatus = -1;
            failure.chunkName = request.chunkName;
            if (!state.main || !state.active) {
                failure.error = "execution is restricted to the active Diesel _main interpreter";
            } else if (source.size() > kMaximumSourceBytes) {
                failure.error = "source exceeds the 2 MiB Workbench safety limit";
            } else {
                failure.error = "Lua execution queue is full";
            }
            g_results[request.id] = failure;
            return request.id;
        }
        g_requests.push_back(request);
    }
    return request.id;
}

std::uint64_t QueueVfsLoad(const StateInfo& state, const std::string& logicalPath) {
    const std::string source = "return dofile(" + EscapeLuaString(logicalPath) + ")";
    return QueueExecute(state, source, "@ScreamSDK/VFS/" + logicalPath);
}

bool TryGetResult(std::uint64_t requestId, ExecutionResult& result) {
    std::lock_guard<std::mutex> lock(g_requestMutex);
    const std::map<std::uint64_t, ExecutionResult>::const_iterator found =
        g_results.find(requestId);
    if (found == g_results.end()) {
        return false;
    }
    result = found->second;
    return true;
}

std::uint64_t QueueEnvironmentSnapshot(const StateInfo& state,
                                       std::uint32_t maximumDepth,
                                       std::uint32_t maximumItems) {
    SnapshotRequest request = {};
    std::lock_guard<std::mutex> lock(g_snapshotMutex);
    request.id = g_nextSnapshotRequestId++;
    request.state = state;
    request.maximumDepth = std::min(maximumDepth, kMaximumSnapshotDepth);
    request.maximumItems = std::max<std::uint32_t>(
        16, std::min(maximumItems, kMaximumSnapshotItems));
    if (!state.main || !state.active ||
        g_snapshotRequests.size() >= kMaximumSnapshotRequests) {
        EnvironmentSnapshot failure = {};
        failure.requestId = request.id;
        failure.complete = true;
        failure.success = false;
        failure.error = !state.main || !state.active
            ? "environment snapshots require the active Diesel _main interpreter"
            : "environment snapshot queue is full";
        g_snapshots[request.id] = failure;
        return request.id;
    }
    g_snapshotRequests.push_back(request);
    return request.id;
}

bool TryGetEnvironmentSnapshot(std::uint64_t requestId,
                               EnvironmentSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(g_snapshotMutex);
    const std::map<std::uint64_t, EnvironmentSnapshot>::const_iterator found =
        g_snapshots.find(requestId);
    if (found == g_snapshots.end()) {
        return false;
    }
    snapshot = found->second;
    return true;
}

void RecordBufferLoad(lua_State* state,
                      const char* chunkName,
                      const char* source,
                      std::uint32_t size) {
    if (state == NULL) {
        return;
    }
    LoadedScript record = {};
    record.state = state;
    record.chunkName = ReadCString(chunkName, 4096);
    record.size = size;
    record.loadCount = 1;
    record.sourceAvailable = source != NULL && size <= kMaximumSourceBytes &&
                             Scream::Memory::IsReadable(source, size);
    if (record.sourceAvailable && size != 0 &&
        (static_cast<unsigned char>(source[0]) == 0x1B ||
         memchr(source, 0, size) != NULL)) {
        record.sourceAvailable = false; // compiled bytecode is metadata-only
    }
    if (record.sourceAvailable) {
        record.source.assign(source, source + size);
    }

    std::lock_guard<std::mutex> lock(g_loadedMutex);
    for (std::size_t i = 0; i < g_loadedScripts.size(); ++i) {
        LoadedScript& existing = g_loadedScripts[i];
        if (existing.state == state && existing.chunkName == record.chunkName) {
            existing.size = size;
            ++existing.loadCount;
            existing.sourceAvailable = false;
            existing.source.clear();
            if (record.sourceAvailable) {
                existing.sourceAvailable = true;
                existing.source.swap(record.source);
            }
            EnforceSourceBudget();
            return;
        }
    }
    if (g_loadedScripts.size() >= kMaximumLoadedScripts) {
        g_loadedScripts.erase(g_loadedScripts.begin());
    }
    g_loadedScripts.push_back(record);
    EnforceSourceBudget();
}

void RecordStreamLoad(lua_State* state, const char* chunkName) {
    if (state == NULL) {
        return;
    }
    const std::string name = ReadCString(chunkName, 4096);
    std::lock_guard<std::mutex> lock(g_loadedMutex);
    for (std::size_t i = 0; i < g_loadedScripts.size(); ++i) {
        LoadedScript& existing = g_loadedScripts[i];
        if (existing.state == state && existing.chunkName == name) {
            ++existing.loadCount;
            existing.size = 0;
            existing.sourceAvailable = false;
            existing.source.clear();
            return;
        }
    }
    LoadedScript record = {};
    record.state = state;
    record.chunkName = name;
    record.loadCount = 1;
    record.sourceAvailable = false;
    if (g_loadedScripts.size() >= kMaximumLoadedScripts) {
        g_loadedScripts.erase(g_loadedScripts.begin());
    }
    g_loadedScripts.push_back(record);
}

} // namespace Lua
} // namespace Diesel
