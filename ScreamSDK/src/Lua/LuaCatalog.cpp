#include "Diesel/Lua.hpp"

#include "Internal.hpp"
#include "ScreamSDK/Log.hpp"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <vector>

namespace Diesel {
namespace Lua {

namespace {

std::mutex g_catalogMutex;
std::vector<ScriptResource> g_catalog;
const std::uint64_t kMaximumIndexBytes = 64ull * 1024ull * 1024ull;
const std::uint64_t kMaximumRecords = 1000000;
const std::uint64_t kMaximumSourceBytes = 8ull * 1024ull * 1024ull;

bool AddFits(std::uint64_t offset, std::uint64_t size, std::uint64_t total) {
    return offset <= total && size <= total - offset;
}

class Reader {
public:
    explicit Reader(const std::vector<unsigned char>& bytes) : bytes_(bytes), position_(0) {}

    void seek(std::size_t position) {
        if (position > bytes_.size()) {
            throw std::runtime_error("index seek outside buffer");
        }
        position_ = position;
    }

    std::size_t position() const { return position_; }

    unsigned char u8() {
        require(1);
        return bytes_[position_++];
    }

    std::uint32_t u32() {
        require(4);
        const std::uint32_t value = static_cast<std::uint32_t>(bytes_[position_]) |
            (static_cast<std::uint32_t>(bytes_[position_ + 1]) << 8) |
            (static_cast<std::uint32_t>(bytes_[position_ + 2]) << 16) |
            (static_cast<std::uint32_t>(bytes_[position_ + 3]) << 24);
        position_ += 4;
        return value;
    }

    std::int32_t i32() { return static_cast<std::int32_t>(u32()); }

    std::uint64_t u64() {
        const std::uint64_t low = u32();
        const std::uint64_t high = u32();
        return low | (high << 32);
    }

    std::string cstring() {
        const std::size_t start = position_;
        while (position_ < bytes_.size() && bytes_[position_] != 0) {
            if (position_ - start >= 65536) {
                throw std::runtime_error("unreasonably long archive string");
            }
            ++position_;
        }
        if (position_ >= bytes_.size()) {
            throw std::runtime_error("unterminated archive string");
        }
        std::string result(reinterpret_cast<const char*>(&bytes_[start]), position_ - start);
        ++position_;
        return result;
    }

private:
    void require(std::size_t count) {
        if (position_ > bytes_.size() || count > bytes_.size() - position_) {
            throw std::runtime_error("truncated archive index");
        }
    }

    const std::vector<unsigned char>& bytes_;
    std::size_t position_;
};

std::uint64_t FileSize(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA data = {};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data) ||
        (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return 0;
    }
    return (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) |
           data.nFileSizeLow;
}

bool ReadPrefix(const std::string& path,
                std::uint64_t count,
                std::vector<unsigned char>& bytes) {
    if (count > kMaximumIndexBytes || count > static_cast<std::uint64_t>(SIZE_MAX)) {
        return false;
    }
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) {
        return false;
    }
    bytes.resize(static_cast<std::size_t>(count));
    if (count != 0) {
        input.read(reinterpret_cast<char*>(&bytes[0]), static_cast<std::streamsize>(count));
        if (input.gcount() != static_cast<std::streamsize>(count)) {
            bytes.clear();
            return false;
        }
    }
    return true;
}

bool HasLuaSuffix(const std::string& path, char finalCharacter) {
    if (path.size() < 4) {
        return false;
    }
    const std::string suffix = path.substr(path.size() - 4);
    return suffix[0] == '.' &&
           std::tolower(static_cast<unsigned char>(suffix[1])) == 'l' &&
           std::tolower(static_cast<unsigned char>(suffix[2])) == 'u' &&
           std::tolower(static_cast<unsigned char>(suffix[3])) == finalCharacter;
}

bool IsLuaScript(const std::string& path) {
    return HasLuaSuffix(path, 'a') || HasLuaSuffix(path, 'c');
}

bool IsCompiledLua(const std::string& path) {
    return HasLuaSuffix(path, 'c');
}

std::string JoinLogical(const std::string& parent, const std::string& child) {
    if (parent.empty()) {
        return child;
    }
    return parent + "/" + child;
}

bool ValidComponent(const std::string& component) {
    return !component.empty() && component != "." && component != ".." &&
           component.find('/') == std::string::npos &&
           component.find('\\') == std::string::npos;
}

void AddBundleFile(const std::string& container,
                   std::uint64_t fileSize,
                   const std::string& path,
                   std::uint64_t offset,
                   std::uint32_t size,
                   bool exists,
                   std::vector<ScriptResource>& output) {
    if (!exists || !IsLuaScript(path)) {
        return;
    }
    if (size != 0 && !AddFits(offset, size, fileSize)) {
        throw std::runtime_error("Lua payload lies outside BNDL file");
    }
    ScriptResource resource = {};
    resource.logicalPath = path;
    resource.containerPath = container;
    resource.archiveOffset = offset;
    resource.size = size;
    resource.compiled = IsCompiledLua(path);
    resource.sourceAvailable = !resource.compiled && size <= kMaximumSourceBytes;
    output.push_back(resource);
}

void ParseBundleScope(Reader& reader,
                      int version,
                      unsigned int depth,
                      std::uint64_t& records,
                      const std::string& parent,
                      const std::string& container,
                      std::uint64_t fileSize,
                      std::vector<ScriptResource>& output) {
    if (depth > 256) {
        throw std::runtime_error("BNDL directory depth exceeds safety limit");
    }
    for (;;) {
        if (++records > kMaximumRecords) {
            throw std::runtime_error("BNDL record count exceeds safety limit");
        }
        bool directory = false;
        bool file = false;
        bool exists = false;
        std::uint64_t offset = 0;
        std::uint32_t size = 0;

        if (version == 2) {
            const unsigned char tag = reader.u8();
            if (tag == 3) {
                return;
            }
            if (tag == 1) {
                directory = true;
                exists = reader.u8() != 0;
            } else if (tag == 2) {
                file = true;
                offset = reader.u64();
                size = reader.u32();
                exists = reader.u8() != 0;
            } else {
                throw std::runtime_error("unsupported BNDL v2 record tag");
            }
        } else if (version == 1) {
            const std::int32_t tag = reader.i32();
            if (tag == -2) {
                return;
            }
            if (tag == -1) {
                directory = true;
                exists = reader.u8() != 0;
            } else if (tag >= 0) {
                file = true;
                offset = static_cast<std::uint32_t>(tag);
                size = reader.u32();
                exists = reader.u8() != 0;
            } else {
                throw std::runtime_error("unsupported BNDL v1 record tag");
            }
        } else {
            throw std::runtime_error("unsupported BNDL version");
        }

        const std::string name = reader.cstring();
        if (!ValidComponent(name)) {
            throw std::runtime_error("invalid BNDL path component");
        }
        const std::string logical = JoinLogical(parent, name);
        if (directory) {
            ParseBundleScope(reader, version, depth + 1, records, logical,
                             container, fileSize, output);
        } else if (file) {
            AddBundleFile(container, fileSize, logical, offset, size, exists, output);
        }
    }
}

void ParseBundle(const std::string& path, std::vector<ScriptResource>& output) {
    const std::uint64_t fileSize = FileSize(path);
    if (fileSize < 16) {
        return;
    }
    std::vector<unsigned char> header;
    if (!ReadPrefix(path, 16, header) || memcmp(&header[0], "BNDL", 4) != 0) {
        return;
    }
    Reader headerReader(header);
    headerReader.seek(4);
    const int version = headerReader.i32();
    const std::int32_t claimedEnd = headerReader.i32();
    if (claimedEnd < 16 || static_cast<std::uint64_t>(claimedEnd) > fileSize) {
        throw std::runtime_error("invalid BNDL index extent");
    }

    std::vector<unsigned char> index;
    if (!ReadPrefix(path, static_cast<std::uint32_t>(claimedEnd), index)) {
        throw std::runtime_error("could not read BNDL index");
    }
    Reader reader(index);
    reader.seek(16);
    std::uint64_t records = 0;
    ParseBundleScope(reader, version, 0, records, std::string(), path, fileSize, output);
    if (reader.position() != static_cast<std::size_t>(claimedEnd)) {
        throw std::runtime_error("BNDL index end does not match parsed records");
    }
}

void ParseLhd(const std::string& path, std::vector<ScriptResource>& output) {
    const std::uint64_t fileSize = FileSize(path);
    if (fileSize < 8 || fileSize > kMaximumIndexBytes) {
        return;
    }
    std::vector<unsigned char> bytes;
    if (!ReadPrefix(path, fileSize, bytes)) {
        return;
    }
    Reader reader(bytes);
    const std::uint32_t stringCount = reader.u32();
    if (stringCount > kMaximumRecords) {
        throw std::runtime_error("LHD string count exceeds safety limit");
    }
    std::vector<std::string> strings;
    strings.reserve(stringCount);
    for (std::uint32_t i = 0; i < stringCount; ++i) {
        strings.push_back(reader.cstring());
    }

    struct Entry {
        std::uint32_t directoryString;
        std::uint32_t filenameString;
        std::uint32_t size;
        std::int32_t defaultLinearIndex;
    };
    const std::uint32_t entryCount = reader.u32();
    if (entryCount > kMaximumRecords) {
        throw std::runtime_error("LHD entry count exceeds safety limit");
    }
    std::vector<Entry> entries;
    entries.reserve(entryCount);
    for (std::uint32_t i = 0; i < entryCount; ++i) {
        Entry entry = {};
        entry.directoryString = reader.u32();
        entry.filenameString = reader.u32();
        entry.size = reader.u32();
        entry.defaultLinearIndex = reader.i32();
        if (entry.directoryString >= strings.size() ||
            entry.filenameString >= strings.size()) {
            throw std::runtime_error("LHD entry contains an invalid string index");
        }
        entries.push_back(entry);
    }

    const std::uint32_t linearCount = reader.u32();
    if (linearCount > kMaximumRecords) {
        throw std::runtime_error("LHD linear-file count exceeds safety limit");
    }
    std::uint64_t totalOccurrences = 0;
    for (std::uint32_t linear = 0; linear < linearCount; ++linear) {
        const std::uint32_t nameString = reader.u32();
        if (nameString >= strings.size()) {
            throw std::runtime_error("LHD linear file contains an invalid name index");
        }
        const std::uint32_t occurrenceCount = reader.u32();
        totalOccurrences += occurrenceCount;
        if (totalOccurrences > kMaximumRecords) {
            throw std::runtime_error("LHD occurrence count exceeds safety limit");
        }
        for (std::uint32_t occurrence = 0; occurrence < occurrenceCount;
             ++occurrence) {
            const std::uint32_t entryIndex = reader.u32();
            if (entryIndex >= entries.size() ||
                entries[entryIndex].defaultLinearIndex < 0) {
                throw std::runtime_error("LHD occurrence contains an invalid entry index");
            }
        }
        for (std::uint32_t occurrence = 0; occurrence < occurrenceCount;
             ++occurrence) {
            reader.u32();
        }
    }

    const std::uint32_t directoryCount = reader.u32();
    if (directoryCount > kMaximumRecords) {
        throw std::runtime_error("LHD directory count exceeds safety limit");
    }
    std::uint64_t totalMembers = 0;
    for (std::uint32_t directory = 0; directory < directoryCount; ++directory) {
        const std::uint32_t pathString = reader.u32();
        if (pathString >= strings.size()) {
            throw std::runtime_error("LHD directory contains an invalid string index");
        }
        const std::uint32_t memberCount = reader.u32();
        totalMembers += memberCount;
        if (totalMembers > kMaximumRecords) {
            throw std::runtime_error("LHD directory-member count exceeds safety limit");
        }
        for (std::uint32_t member = 0; member < memberCount; ++member) {
            if (reader.u32() >= entries.size()) {
                throw std::runtime_error("LHD directory contains an invalid entry index");
            }
        }
    }
    if (reader.position() != bytes.size()) {
        throw std::runtime_error("unexpected trailing bytes in LHD descriptor");
    }

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const Entry& entry = entries[i];
        if (entry.defaultLinearIndex < 0) {
            continue;
        }
        if (static_cast<std::uint32_t>(entry.defaultLinearIndex) >= linearCount) {
            throw std::runtime_error("LHD entry contains an invalid linear-file index");
        }
        std::string directory = strings[entry.directoryString];
        const std::string& name = strings[entry.filenameString];
        std::replace(directory.begin(), directory.end(), '\\', '/');
        while (!directory.empty() && directory[directory.size() - 1] == '/') {
            directory.erase(directory.size() - 1);
        }
        const std::string logical = JoinLogical(directory, name);
        if (IsLuaScript(logical)) {
            ScriptResource resource = {};
            resource.logicalPath = logical;
            resource.containerPath = path;
            resource.size = entry.size;
            resource.compiled = IsCompiledLua(logical);
            resource.sourceAvailable = false;
            output.push_back(resource);
        }
    }
}

std::string ExecutableDirectory() {
    char path[32768] = {};
    const DWORD length = GetModuleFileNameA(NULL, path, sizeof(path));
    if (length == 0 || length >= sizeof(path)) {
        return std::string();
    }
    std::string result(path, length);
    const std::string::size_type slash = result.find_last_of("\\/");
    return slash == std::string::npos ? std::string() : result.substr(0, slash + 1);
}

} // namespace

std::vector<ScriptResource> CatalogScripts() {
    std::lock_guard<std::mutex> lock(g_catalogMutex);
    return g_catalog;
}

bool LoadCatalogSource(const ScriptResource& resource,
                       std::string& source,
                       std::string& error) {
    source.clear();
    ScriptResource matched = {};
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(g_catalogMutex);
        for (std::size_t i = 0; i < g_catalog.size(); ++i) {
            if (g_catalog[i].logicalPath == resource.logicalPath &&
                g_catalog[i].containerPath == resource.containerPath &&
                g_catalog[i].archiveOffset == resource.archiveOffset &&
                g_catalog[i].size == resource.size) {
                matched = g_catalog[i];
                found = true;
                break;
            }
        }
    }
    if (!found || !matched.sourceAvailable) {
        error = "source is not directly available from this catalog entry";
        return false;
    }
    const std::uint64_t fileSize = FileSize(matched.containerPath);
    if (matched.size > kMaximumSourceBytes ||
        !AddFits(matched.archiveOffset, matched.size, fileSize)) {
        error = "catalog payload range is no longer valid";
        return false;
    }
    std::ifstream input(matched.containerPath.c_str(), std::ios::binary);
    if (!input) {
        error = "could not open the source bundle";
        return false;
    }
    input.seekg(static_cast<std::streamoff>(matched.archiveOffset));
    source.resize(matched.size);
    if (matched.size != 0) {
        input.read(&source[0], matched.size);
        if (input.gcount() != static_cast<std::streamsize>(matched.size)) {
            source.clear();
            error = "short read while loading script source";
            return false;
        }
    }
    error.clear();
    return true;
}

} // namespace Lua
} // namespace Diesel

namespace Scream {
namespace LuaCatalogInternal {

void Discover() {
    std::vector<Diesel::Lua::ScriptResource> discovered;
    const std::string base = Diesel::Lua::ExecutableDirectory();
    const char* relativeCandidates[] = {
        "quick.bundle", "stream.bundle", "game.lhd",
        "bundles\\quick.bundle", "bundles\\stream.bundle", "bundles\\game.lhd",
        "data\\quick.bundle", "data\\stream.bundle", "data\\game.lhd"
    };
    std::set<std::string> visited;
    for (std::size_t i = 0;
         i < sizeof(relativeCandidates) / sizeof(relativeCandidates[0]); ++i) {
        const std::string path = base + relativeCandidates[i];
        if (Diesel::Lua::FileSize(path) == 0 || !visited.insert(path).second) {
            continue;
        }
        try {
            const std::string suffix = path.size() >= 4 ? path.substr(path.size() - 4) : "";
            if (suffix == ".lhd") {
                Diesel::Lua::ParseLhd(path, discovered);
            } else {
                Diesel::Lua::ParseBundle(path, discovered);
            }
        } catch (const std::exception& exception) {
            SCREAM_LOG_WARN("Could not catalog %s: %s", path.c_str(), exception.what());
        }
    }
    std::sort(discovered.begin(), discovered.end(),
              [](const Diesel::Lua::ScriptResource& left,
                 const Diesel::Lua::ScriptResource& right) {
                  if (left.logicalPath != right.logicalPath) {
                      return left.logicalPath < right.logicalPath;
                  }
                  return left.containerPath < right.containerPath;
              });
    {
        std::lock_guard<std::mutex> lock(Diesel::Lua::g_catalogMutex);
        Diesel::Lua::g_catalog.swap(discovered);
    }
    SCREAM_LOG_INFO("Cataloged %u loadable Lua resources from known BNDL/LHD indexes",
                    static_cast<unsigned int>(Diesel::Lua::g_catalog.size()));
}

void Clear() {
    std::lock_guard<std::mutex> lock(Diesel::Lua::g_catalogMutex);
    Diesel::Lua::g_catalog.clear();
}

} // namespace LuaCatalogInternal
} // namespace Scream
