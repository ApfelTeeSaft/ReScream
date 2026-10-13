#pragma once

#include "ScreamSDK/Platform.hpp"

#include <cstdint>
#include <string>

namespace Scream {

struct GameBuild {
    std::uint16_t machine;
    std::uint32_t timeDateStamp;
    std::uint32_t imageSize;
    std::uint32_t entryPointRva;
    std::string pdbGuid;
    std::uint32_t pdbAge;
    std::string textSha256;
};

namespace Build {

extern SCREAM_API const GameBuild kScreamPublicDev;

SCREAM_API bool FingerprintRunningGame(GameBuild& result, std::string& error);
SCREAM_API bool MatchesSupportedBuild(const GameBuild& build, std::string& mismatch);
SCREAM_API std::uintptr_t GameBase();
SCREAM_API std::uint32_t GameImageSize();

template <typename T>
T ResolveRva(std::uintptr_t rva) {
    return reinterpret_cast<T>(GameBase() + rva);
}

} // namespace Build
} // namespace Scream
