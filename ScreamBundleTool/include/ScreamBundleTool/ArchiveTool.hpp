#pragma once

#if defined(_MSC_VER) && _MSC_VER < 1910
#include <experimental/filesystem>
#else
#include <filesystem>
#endif

namespace Scream {
namespace ArchiveTool {

#if defined(_MSC_VER) && _MSC_VER < 1910
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif

struct Options {
    bool verbose = false;
    bool force = false;
    bool strict = false;
};

enum class Format {
    bundle,
    fcl,
    lhd,
};

Format DetectFormat(const fs::path& path);
void PrintInfo(const fs::path& archive, const Options& options);
void PrintList(const fs::path& archive, const Options& options);
void Extract(const fs::path& archive,
             const fs::path& outputDirectory,
             const Options& options);
void Pack(const fs::path& inputDirectory,
          const fs::path& outputArchive,
          const Options& options);
void Repack(const fs::path& originalArchive,
            const fs::path& modifiedDirectory,
            const fs::path& outputArchive,
            const Options& options);

bool Verify(const fs::path& originalArchive,
            const fs::path& candidateArchive,
            const Options& options);

} // namespace ArchiveTool
} // namespace Scream
