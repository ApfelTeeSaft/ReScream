#include "ScreamSDK/Log.hpp"

#include <Windows.h>

#include <cstdarg>
#include <cstdio>
#include <deque>
#include <mutex>
#if defined(_MSC_VER)
#include <share.h>
#endif

namespace Scream {
namespace Log {

namespace {

std::mutex g_mutex;
FILE* g_file = NULL;
std::deque<std::string> g_lines;
const std::size_t kMaximumLines = 1500;

const char* LevelName(Level level) {
    switch (level) {
    case Debug: return "DEBUG";
    case Info: return "INFO";
    case Warning: return "WARN";
    case Error: return "ERROR";
    default: return "?";
    }
}

std::string ModuleDirectory(void* module) {
    char path[32768] = {};
    const DWORD length = GetModuleFileNameA(static_cast<HMODULE>(module), path,
                                            sizeof(path));
    if (length == 0 || length >= sizeof(path)) {
        return std::string();
    }
    std::string result(path, length);
    const std::string::size_type slash = result.find_last_of("\\/");
    return slash == std::string::npos ? std::string() : result.substr(0, slash + 1);
}

} // namespace

bool Initialize(void* dllModule) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file != NULL) {
        return true;
    }
    const std::string path = ModuleDirectory(dllModule) + "ScreamSDK.log";
#if defined(_MSC_VER)
    g_file = _fsopen(path.c_str(), "a", _SH_DENYNO);
#else
    g_file = fopen(path.c_str(), "a");
#endif
    return g_file != NULL;
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file != NULL) {
        fflush(g_file);
        fclose(g_file);
        g_file = NULL;
    }
}

void Write(Level level, const char* format, ...) {
    char message[4096] = {};
    va_list arguments;
    va_start(arguments, format);
#if defined(_MSC_VER)
    _vsnprintf_s(message, sizeof(message), _TRUNCATE, format, arguments);
#else
    vsnprintf(message, sizeof(message), format, arguments);
#endif
    va_end(arguments);

    SYSTEMTIME now = {};
    GetLocalTime(&now);
    char line[4352] = {};
#if defined(_MSC_VER)
    _snprintf_s(line, sizeof(line), _TRUNCATE,
                "%02u:%02u:%02u.%03u [%s] %s\n",
                now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
                LevelName(level), message);
#else
    snprintf(line, sizeof(line), "%02u:%02u:%02u.%03u [%s] %s\n",
             now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
             LevelName(level), message);
#endif
    OutputDebugStringA(line);

    std::lock_guard<std::mutex> lock(g_mutex);
    g_lines.push_back(line);
    while (g_lines.size() > kMaximumLines) {
        g_lines.pop_front();
    }
    if (g_file != NULL) {
        fputs(line, g_file);
        fflush(g_file);
    }
}

std::vector<std::string> Snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return std::vector<std::string>(g_lines.begin(), g_lines.end());
}

} // namespace Log
} // namespace Scream
