#pragma once

#include "ScreamSDK/Platform.hpp"

#include <string>
#include <vector>

namespace Scream {
namespace Log {

enum Level {
    Debug,
    Info,
    Warning,
    Error,
};

SCREAM_API bool Initialize(void* dllModule);
SCREAM_API void Shutdown();
SCREAM_API void Write(Level level, const char* format, ...);
SCREAM_API std::vector<std::string> Snapshot();

} // namespace Log
} // namespace Scream

#define SCREAM_LOG_DEBUG(...) ::Scream::Log::Write(::Scream::Log::Debug, __VA_ARGS__)
#define SCREAM_LOG_INFO(...) ::Scream::Log::Write(::Scream::Log::Info, __VA_ARGS__)
#define SCREAM_LOG_WARN(...) ::Scream::Log::Write(::Scream::Log::Warning, __VA_ARGS__)
#define SCREAM_LOG_ERROR(...) ::Scream::Log::Write(::Scream::Log::Error, __VA_ARGS__)
