#pragma once

#include <Windows.h>

namespace Client {
namespace Log {

enum Level {
    Debug,
    Info,
    Warning,
    Error
};

bool Initialize(HMODULE module);
void Shutdown();
void Write(Level level, const char* format, ...);

} // namespace Log
} // namespace Client

#define CLIENT_LOG_DEBUG(...) \
    ::Client::Log::Write(::Client::Log::Debug, __VA_ARGS__)
#define CLIENT_LOG_INFO(...) \
    ::Client::Log::Write(::Client::Log::Info, __VA_ARGS__)
#define CLIENT_LOG_WARNING(...) \
    ::Client::Log::Write(::Client::Log::Warning, __VA_ARGS__)
#define CLIENT_LOG_ERROR(...) \
    ::Client::Log::Write(::Client::Log::Error, __VA_ARGS__)
