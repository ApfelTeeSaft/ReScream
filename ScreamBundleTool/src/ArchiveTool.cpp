#include "ScreamBundleTool/ArchiveTool.hpp"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Scream {
namespace ArchiveTool {
namespace {

constexpr std::uint64_t kCopyBufferSize = 1024 * 1024;
constexpr std::uint32_t kLinearSegmentSize = 0x10000;
constexpr std::uint64_t kMaximumRecordCount = 10'000'000;
constexpr std::size_t kMaximumStringLength = 1024 * 1024;

[[noreturn]] void Fail(const std::string& message) {
    throw std::runtime_error(message);
}

std::string Quote(const fs::path& path) {
    return "'" + path.u8string() + "'";
}

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string ComparableAbsolute(const fs::path& path) {
    std::error_code error;
    fs::path normalized;
    if (fs::exists(path, error) && !error) {
        normalized = fs::canonical(path, error);
    } else {
        error.clear();
        const auto parent = path.parent_path().empty() ? fs::path(".")
                                                       : path.parent_path();
        const auto canonicalParent = fs::canonical(parent, error);
        if (!error) {
            normalized = canonicalParent / path.filename();
        }
    }
    if (error || normalized.empty()) {
        normalized = fs::absolute(path);
    }
    auto result = normalized.generic_u8string();
    while (result.size() > 1 && result.back() == '/') {
        result.pop_back();
    }
    return LowerAscii(result);
}

std::string RelativeContainedPath(const fs::path& child, const fs::path& root) {
    std::error_code error;
    const auto canonicalRoot = fs::canonical(root, error);
    if (error) {
        Fail("cannot canonicalize input directory " + Quote(root));
    }
    const auto canonicalChild = fs::canonical(child, error);
    if (error) {
        Fail("cannot canonicalize input file " + Quote(child));
    }
    auto rootString = canonicalRoot.generic_u8string();
    auto childString = canonicalChild.generic_u8string();
    if (rootString.empty() || rootString.back() != '/') {
        rootString.push_back('/');
    }
    const auto lowerRoot = LowerAscii(rootString);
    const auto lowerChild = LowerAscii(childString);
    if (lowerChild.size() < lowerRoot.size() ||
        lowerChild.compare(0, lowerRoot.size(), lowerRoot) != 0) {
        Fail("modified file escapes the requested input directory: " + Quote(child));
    }
    return childString.substr(rootString.size());
}

std::string ExtensionLower(const fs::path& path) {
    return LowerAscii(path.extension().u8string());
}

std::uint64_t CheckedAdd(std::uint64_t left,
                         std::uint64_t right,
                         const char* description) {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        Fail(std::string("integer overflow while computing ") + description);
    }
    return left + right;
}

std::uint64_t Align4(std::uint64_t value) {
    return CheckedAdd(value, (4 - (value & 3)) & 3, "four-byte alignment");
}

class BinaryReader {
public:
    explicit BinaryReader(fs::path path)
        : path_(std::move(path)), stream_(path_, std::ios::binary) {
        if (!stream_) {
            Fail("cannot open " + Quote(path_));
        }
        stream_.seekg(0, std::ios::end);
        const auto end = stream_.tellg();
        if (end < 0) {
            Fail("cannot determine size of " + Quote(path_));
        }
        size_ = static_cast<std::uint64_t>(end);
        stream_.seekg(0, std::ios::beg);
    }

    std::uint64_t size() const { return size_; }

    std::uint64_t tell() {
        const auto result = stream_.tellg();
        if (result < 0) {
            Fail("cannot query position in " + Quote(path_));
        }
        return static_cast<std::uint64_t>(result);
    }

    void seek(std::uint64_t offset) {
        if (offset > size_) {
            Fail("seek outside " + Quote(path_));
        }
        stream_.clear();
        stream_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!stream_) {
            Fail("seek failed in " + Quote(path_));
        }
    }

    void read(void* destination, std::size_t count) {
        const auto position = tell();
        if (count > size_ - position) {
            Fail("truncated data at offset 0x" + Hex(position) + " in " + Quote(path_));
        }
        if (count == 0) {
            return;
        }
        stream_.read(static_cast<char*>(destination), static_cast<std::streamsize>(count));
        if (!stream_) {
            Fail("read failed in " + Quote(path_));
        }
    }

    std::vector<std::uint8_t> readBytes(std::size_t count) {
        std::vector<std::uint8_t> bytes(count);
        read(bytes.data(), count);
        return bytes;
    }

    std::uint8_t u8() {
        std::uint8_t value = 0;
        read(&value, sizeof(value));
        return value;
    }

    std::uint32_t u32() {
        std::array<std::uint8_t, 4> bytes{};
        read(bytes.data(), bytes.size());
        return static_cast<std::uint32_t>(bytes[0]) |
               (static_cast<std::uint32_t>(bytes[1]) << 8) |
               (static_cast<std::uint32_t>(bytes[2]) << 16) |
               (static_cast<std::uint32_t>(bytes[3]) << 24);
    }

    std::int32_t i32() { return static_cast<std::int32_t>(u32()); }

    std::uint64_t u64() {
        const std::uint64_t low = u32();
        const std::uint64_t high = u32();
        return low | (high << 32);
    }

    std::string cstring() {
        std::string value;
        while (true) {
            if (value.size() >= kMaximumStringLength) {
                Fail("unreasonably long NUL-terminated string in " + Quote(path_));
            }
            const auto character = u8();
            if (character == 0) {
                return value;
            }
            value.push_back(static_cast<char>(character));
        }
    }

    static std::string Hex(std::uint64_t value) {
        std::ostringstream stream;
        stream << std::hex << std::uppercase << value;
        return stream.str();
    }

private:
    fs::path path_;
    std::ifstream stream_;
    std::uint64_t size_ = 0;
};

void AppendU8(std::vector<std::uint8_t>& output, std::uint8_t value) {
    output.push_back(value);
}

void AppendU32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    for (unsigned shift = 0; shift != 32; shift += 8) {
        output.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void AppendU64(std::vector<std::uint8_t>& output, std::uint64_t value) {
    AppendU32(output, static_cast<std::uint32_t>(value));
    AppendU32(output, static_cast<std::uint32_t>(value >> 32));
}

void AppendCString(std::vector<std::uint8_t>& output, const std::string& value) {
    output.insert(output.end(), value.begin(), value.end());
    output.push_back(0);
}

void WriteU32(std::ostream& output, std::uint32_t value) {
    std::array<char, 4> bytes{};
    for (unsigned index = 0; index != 4; ++index) {
        bytes[index] = static_cast<char>(value >> (index * 8));
    }
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void WriteBytes(std::ostream& output,
                const std::vector<std::uint8_t>& bytes) {
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
}

void CopyRange(const fs::path& source,
               std::uint64_t offset,
               std::uint64_t size,
               std::ostream& output) {
    if (size == 0) {
        return;
    }
    BinaryReader reader(source);
    if (CheckedAdd(offset, size, "source range") > reader.size()) {
        Fail("source range is outside " + Quote(source));
    }
    reader.seek(offset);
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(
        std::min<std::uint64_t>(kCopyBufferSize, std::max<std::uint64_t>(size, 1))));
    while (size != 0) {
        const auto chunk = static_cast<std::size_t>(
            std::min<std::uint64_t>(size, buffer.size()));
        reader.read(buffer.data(), chunk);
        output.write(reinterpret_cast<const char*>(buffer.data()),
                     static_cast<std::streamsize>(chunk));
        if (!output) {
            Fail("write failed while copying from " + Quote(source));
        }
        size -= chunk;
    }
}

fs::path TemporarySibling(const fs::path& output) {
    const auto stamp = std::chrono::high_resolution_clock::now()
                           .time_since_epoch()
                           .count();
    for (unsigned attempt = 0; attempt != 1000; ++attempt) {
        auto candidate = output;
        candidate += ".tmp." + std::to_string(stamp) + "." + std::to_string(attempt);
        if (!fs::exists(candidate)) {
            return candidate;
        }
    }
    Fail("cannot allocate a temporary output path beside " + Quote(output));
}

void CommitTemporary(const fs::path& temporary,
                     const fs::path& output,
                     bool force) {
    std::error_code error;
    if (fs::exists(output)) {
        if (!force) {
            Fail(Quote(output) + " already exists (use --force to replace it)");
        }
        if (!fs::is_regular_file(output)) {
            Fail("refusing to replace non-file output " + Quote(output));
        }
        if (!fs::remove(output, error) || error) {
            Fail("cannot replace " + Quote(output) + ": " + error.message());
        }
    }
    fs::rename(temporary, output, error);
    if (error) {
        Fail("cannot commit " + Quote(output) + ": " + error.message());
    }
}

void EnsureOutputParent(const fs::path& output) {
    const auto parent = output.parent_path();
    if (!parent.empty()) {
        std::error_code error;
        fs::create_directories(parent, error);
        if (error) {
            Fail("cannot create " + Quote(parent) + ": " + error.message());
        }
    }
}

bool IsReservedWindowsName(const std::string& value) {
    auto base = LowerAscii(value.substr(0, value.find('.')));
    static const std::set<std::string> fixed = {"con", "prn", "aux", "nul"};
    if (fixed.count(base) != 0) {
        return true;
    }
    if (base.size() == 4 && (base.rfind("com", 0) == 0 || base.rfind("lpt", 0) == 0) &&
        base[3] >= '1' && base[3] <= '9') {
        return true;
    }
    return false;
}

std::vector<std::string> SplitLogicalPath(const std::string& raw) {
    std::vector<std::string> components;
    std::string component;
    auto finish = [&] {
        if (component.empty() || component == "." || component == "..") {
            Fail("unsafe empty/dot path component in archive path '" + raw + "'");
        }
        if (component.back() == '.' || component.back() == ' ') {
            Fail("unsafe trailing dot/space in archive path '" + raw + "'");
        }
        if (component.find_first_of("<>:\"|?*") != std::string::npos ||
            IsReservedWindowsName(component)) {
            Fail("unsafe Windows path component in archive path '" + raw + "'");
        }
        components.push_back(component);
        component.clear();
    };
    if (raw.empty() || raw.front() == '/' || raw.front() == '\\') {
        Fail("empty or rooted archive path '" + raw + "'");
    }
    for (char character : raw) {
        if (character == '/' || character == '\\') {
            finish();
        } else if (character == '\0') {
            Fail("NUL in logical path");
        } else {
            component.push_back(character);
        }
    }
    finish();
    return components;
}

fs::path SafeOutputPath(const fs::path& root, const std::string& logicalPath) {
    fs::path result = root;
    for (const auto& component : SplitLogicalPath(logicalPath)) {
        result /= fs::u8path(component);
    }
    return result;
}

std::string JoinLogical(const std::string& prefix, const std::string& name) {
    return prefix.empty() ? name : prefix + "\\" + name;
}

void WriteExtractedFile(const fs::path& destination,
                        const Options& options,
                        const std::function<void(std::ostream&)>& writer) {
    EnsureOutputParent(destination);
    if (fs::exists(destination) && !options.force) {
        Fail(Quote(destination) + " already exists (use --force to replace files)");
    }
    const auto temporary = TemporarySibling(destination);
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            Fail("cannot create " + Quote(temporary));
        }
        writer(output);
        output.close();
        if (!output) {
            Fail("write failed for " + Quote(temporary));
        }
        CommitTemporary(temporary, destination, options.force);
    } catch (...) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        throw;
    }
}

enum class BundleNodeKind { directory, file };
enum class PayloadKind { none, archiveRange, diskFile };

struct BundleNode {
    BundleNodeKind kind = BundleNodeKind::directory;
    std::string name;
    bool exists = true;
    std::uint64_t offset = 0;
    std::uint32_t size = 0;
    std::vector<BundleNode> children;
    PayloadKind payloadKind = PayloadKind::none;
    fs::path payloadPath;
    std::uint64_t payloadOffset = 0;
};

struct Bundle {
    fs::path path;
    std::int32_t version = 0;
    std::int32_t claimedIndexEnd = 0;
    std::int32_t reserved = 0;
    std::uint64_t indexEnd = 0;
    std::uint64_t fileSize = 0;
    BundleNode root;
};

BundleNode ReadBundleFileV2(BinaryReader& reader) {
    BundleNode node;
    node.kind = BundleNodeKind::file;
    node.offset = reader.u64();
    node.size = reader.u32();
    node.exists = reader.u8() != 0;
    node.name = reader.cstring();
    return node;
}

std::vector<BundleNode> ReadBundleScope(BinaryReader& reader,
                                        std::int32_t version,
                                        unsigned depth,
                                        std::uint64_t& recordCount) {
    if (depth > 256) {
        Fail("bundle directory recursion exceeds 256 levels");
    }
    std::vector<BundleNode> children;
    while (true) {
        if (++recordCount > kMaximumRecordCount) {
            Fail("bundle contains an unreasonable number of records");
        }
        BundleNode node;
        if (version == 2) {
            const auto tag = reader.u8();
            if (tag == 3) {
                break;
            }
            if (tag == 1) {
                node.kind = BundleNodeKind::directory;
                node.exists = reader.u8() != 0;
                node.name = reader.cstring();
                node.children = ReadBundleScope(reader, version, depth + 1, recordCount);
            } else if (tag == 2) {
                node = ReadBundleFileV2(reader);
            } else {
                Fail("unknown BNDL v2 record tag " + std::to_string(tag));
            }
        } else if (version == 1) {
            const auto word = reader.i32();
            if (word == -2) {
                break;
            }
            if (word == -1) {
                node.kind = BundleNodeKind::directory;
                node.exists = reader.u8() != 0;
                node.name = reader.cstring();
                node.children = ReadBundleScope(reader, version, depth + 1, recordCount);
            } else if (word >= 0) {
                node.kind = BundleNodeKind::file;
                node.offset = static_cast<std::uint32_t>(word);
                node.size = reader.u32();
                node.exists = reader.u8() != 0;
                node.name = reader.cstring();
            } else {
                Fail("unknown negative BNDL v1 record word " + std::to_string(word));
            }
        } else {
            Fail("unsupported BNDL version " + std::to_string(version));
        }
        if (node.name.empty() || node.name.find_first_of("/\\") != std::string::npos) {
            Fail("invalid BNDL component name");
        }
        children.push_back(std::move(node));
    }
    return children;
}

void ValidateBundleNode(const BundleNode& node,
                        const std::string& prefix,
                        const Bundle& bundle,
                        std::set<std::string>& paths,
                        std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges) {
    const auto logical = JoinLogical(prefix, node.name);
    (void)SplitLogicalPath(logical);
    const auto key = LowerAscii(logical);
    if (!paths.insert(key).second) {
        Fail("duplicate/case-colliding BNDL path '" + logical + "'");
    }
    if (node.kind == BundleNodeKind::directory) {
        for (const auto& child : node.children) {
            ValidateBundleNode(child, logical, bundle, paths, ranges);
        }
        return;
    }
    if (node.size != 0) {
        const auto end = CheckedAdd(node.offset, node.size, "BNDL payload range");
        if (end > bundle.fileSize) {
            Fail("BNDL payload for '" + logical + "' exceeds EOF (offset=0x" +
                 BinaryReader::Hex(node.offset) + ", size=" +
                 std::to_string(node.size) + ", file_size=" +
                 std::to_string(bundle.fileSize) + ", exists=" +
                 std::to_string(node.exists ? 1 : 0) + ")");
        }
        if (node.offset < bundle.indexEnd) {
            Fail("BNDL payload for '" + logical + "' overlaps the index");
        }
        ranges.emplace_back(node.offset, end);
    }
}

Bundle ReadBundle(const fs::path& path) {
    BinaryReader reader(path);
    if (reader.size() < 16) {
        Fail("BNDL header is truncated in " + Quote(path));
    }
    const auto tag = reader.readBytes(4);
    if (!std::equal(tag.begin(), tag.end(), "BNDL")) {
        Fail("BNDL magic is absent in " + Quote(path));
    }
    Bundle bundle;
    bundle.path = path;
    bundle.version = reader.i32();
    bundle.claimedIndexEnd = reader.i32();
    bundle.reserved = reader.i32();
    bundle.fileSize = reader.size();
    bundle.root.name.clear();
    bundle.root.kind = BundleNodeKind::directory;
    std::uint64_t recordCount = 0;
    bundle.root.children = ReadBundleScope(reader, bundle.version, 0, recordCount);
    bundle.indexEnd = reader.tell();
    if (bundle.claimedIndexEnd < 16 ||
        static_cast<std::uint64_t>(bundle.claimedIndexEnd) != bundle.indexEnd) {
        Fail("BNDL header.size does not equal the parsed index end");
    }

    std::set<std::string> paths;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    for (const auto& child : bundle.root.children) {
        ValidateBundleNode(child, "", bundle, paths, ranges);
    }
    std::sort(ranges.begin(), ranges.end());
    for (std::size_t index = 1; index < ranges.size(); ++index) {
        if (ranges[index].first < ranges[index - 1].second) {
            Fail("overlapping nonempty BNDL payload ranges");
        }
    }
    return bundle;
}

struct BundleStats {
    std::uint64_t files = 0;
    std::uint64_t directories = 0;
    std::uint64_t zeroSizeFiles = 0;
    std::uint64_t storedBytes = 0;
};

void VisitBundle(const BundleNode& node,
                 const std::string& prefix,
                 const std::function<void(const BundleNode&, const std::string&)>& visitor) {
    const auto logical = node.name.empty() ? prefix : JoinLogical(prefix, node.name);
    if (!node.name.empty()) {
        visitor(node, logical);
    }
    if (node.kind == BundleNodeKind::directory) {
        for (const auto& child : node.children) {
            VisitBundle(child, logical, visitor);
        }
    }
}

BundleStats GetBundleStats(const Bundle& bundle) {
    BundleStats stats;
    VisitBundle(bundle.root, "", [&](const BundleNode& node, const std::string&) {
        if (node.kind == BundleNodeKind::directory) {
            ++stats.directories;
        } else {
            ++stats.files;
            stats.storedBytes += node.size;
            stats.zeroSizeFiles += node.size == 0 ? 1 : 0;
        }
    });
    return stats;
}

void SortBundleTree(BundleNode& node) {
    if (node.kind != BundleNodeKind::directory) {
        return;
    }
    std::sort(node.children.begin(), node.children.end(),
              [](const BundleNode& left, const BundleNode& right) {
                  return left.name < right.name;
              });
    for (auto& child : node.children) {
        SortBundleTree(child);
    }
}

void SerializeBundleScope(const BundleNode& directory,
                          std::vector<std::uint8_t>& output) {
    for (const auto& node : directory.children) {
        if (node.kind == BundleNodeKind::directory) {
            AppendU8(output, 1);
            AppendU8(output, node.exists ? 1 : 0);
            AppendCString(output, node.name);
            SerializeBundleScope(node, output);
        } else {
            AppendU8(output, 2);
            AppendU64(output, node.offset);
            AppendU32(output, node.size);
            AppendU8(output, node.exists ? 1 : 0);
            AppendCString(output, node.name);
        }
    }
    AppendU8(output, 3);
}

void CollectBundleFiles(BundleNode& node, std::vector<BundleNode*>& files) {
    if (node.kind == BundleNodeKind::file) {
        files.push_back(&node);
        return;
    }
    for (auto& child : node.children) {
        CollectBundleFiles(child, files);
    }
}

std::uint32_t CheckedFileSize(const fs::path& path) {
    std::error_code error;
    const auto size = fs::file_size(path, error);
    if (error) {
        Fail("cannot determine size of " + Quote(path) + ": " + error.message());
    }
    if (size > std::numeric_limits<std::uint32_t>::max()) {
        Fail("BNDL entries are limited to 4 GiB: " + Quote(path));
    }
    return static_cast<std::uint32_t>(size);
}

BundleNode BuildBundleDirectory(const fs::path& directory) {
    if (!fs::is_directory(directory)) {
        Fail("input is not a directory: " + Quote(directory));
    }
    BundleNode root;
    root.kind = BundleNodeKind::directory;
    for (const auto& item : fs::directory_iterator(directory)) {
        BundleNode node;
        node.name = item.path().filename().u8string();
        if (node.name.empty() || node.name.find_first_of("/\\") != std::string::npos) {
            Fail("unsupported filesystem name in " + Quote(item.path()));
        }
        if (fs::is_directory(item.path())) {
            node = BuildBundleDirectory(item.path());
            node.name = item.path().filename().u8string();
        } else if (fs::is_regular_file(item.path())) {
            node.kind = BundleNodeKind::file;
            node.exists = true;
            node.size = CheckedFileSize(item.path());
            node.payloadKind = PayloadKind::diskFile;
            node.payloadPath = item.path();
        } else {
            Fail("refusing to pack non-regular filesystem object " + Quote(item.path()));
        }
        root.children.push_back(std::move(node));
    }
    SortBundleTree(root);
    return root;
}

BundleNode* FindBundleChild(BundleNode& directory,
                            const std::string& name,
                            BundleNodeKind kind) {
    auto match = std::find_if(directory.children.begin(), directory.children.end(),
                              [&](const BundleNode& child) {
                                  return child.name == name;
                              });
    if (match == directory.children.end()) {
        return nullptr;
    }
    if (match->kind != kind) {
        Fail("file/directory type conflict for bundle component '" + name + "'");
    }
    return &*match;
}

void ApplyBundleOverlay(BundleNode& target,
                        const fs::path& directory,
                        const std::string& logicalPrefix) {
    for (const auto& item : fs::directory_iterator(directory)) {
        const auto name = item.path().filename().u8string();
        const auto logical = JoinLogical(logicalPrefix, name);
        (void)SplitLogicalPath(logical);
        if (fs::is_directory(item.path())) {
            auto* child = FindBundleChild(target, name, BundleNodeKind::directory);
            if (child == nullptr) {
                BundleNode added;
                added.kind = BundleNodeKind::directory;
                added.name = name;
                added.exists = true;
                target.children.push_back(std::move(added));
                child = &target.children.back();
            }
            child->exists = true;
            ApplyBundleOverlay(*child, item.path(), logical);
        } else if (fs::is_regular_file(item.path())) {
            auto* child = FindBundleChild(target, name, BundleNodeKind::file);
            if (child == nullptr) {
                BundleNode added;
                added.kind = BundleNodeKind::file;
                added.name = name;
                added.exists = true;
                target.children.push_back(std::move(added));
                child = &target.children.back();
            }
            child->exists = true;
            child->size = CheckedFileSize(item.path());
            child->payloadKind = PayloadKind::diskFile;
            child->payloadPath = item.path();
            child->payloadOffset = 0;
        } else {
            Fail("refusing to repack non-regular filesystem object " + Quote(item.path()));
        }
    }
}

void SetOriginalBundleSources(BundleNode& node, const fs::path& original) {
    if (node.kind == BundleNodeKind::file) {
        node.payloadKind = PayloadKind::archiveRange;
        node.payloadPath = original;
        node.payloadOffset = node.offset;
        return;
    }
    for (auto& child : node.children) {
        SetOriginalBundleSources(child, original);
    }
}

void WriteBundle(BundleNode root,
                 std::int32_t reserved,
                 const fs::path& outputPath,
                 const Options& options) {
    SortBundleTree(root);
    std::vector<BundleNode*> files;
    CollectBundleFiles(root, files);

    std::vector<std::uint8_t> provisional;
    SerializeBundleScope(root, provisional);
    const auto indexEnd = CheckedAdd(16, provisional.size(), "BNDL index size");
    if (indexEnd > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
        Fail("BNDL index exceeds the signed 32-bit header field");
    }
    auto cursor = Align4(indexEnd);
    for (auto* file : files) {
        file->offset = cursor;
        if (file->size != 0) {
            cursor = CheckedAdd(cursor, file->size, "BNDL payload layout");
            cursor = Align4(cursor);
        }
    }

    std::vector<std::uint8_t> index;
    SerializeBundleScope(root, index);
    EnsureOutputParent(outputPath);
    const auto temporary = TemporarySibling(outputPath);
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            Fail("cannot create " + Quote(temporary));
        }
        output.write("BNDL", 4);
        WriteU32(output, 2);
        WriteU32(output, static_cast<std::uint32_t>(indexEnd));
        WriteU32(output, static_cast<std::uint32_t>(reserved));
        WriteBytes(output, index);
        for (const auto* file : files) {
            const auto position = static_cast<std::uint64_t>(output.tellp());
            if (position > file->offset) {
                Fail("internal BNDL writer offset regression");
            }
            for (auto count = file->offset - position; count != 0; --count) {
                output.put('\0');
            }
            if (file->size == 0) {
                continue;
            }
            if (file->payloadKind == PayloadKind::archiveRange) {
                CopyRange(file->payloadPath, file->payloadOffset, file->size, output);
            } else if (file->payloadKind == PayloadKind::diskFile) {
                CopyRange(file->payloadPath, 0, file->size, output);
            } else {
                Fail("no payload source for BNDL file '" + file->name + "'");
            }
        }
        output.close();
        if (!output) {
            Fail("write failed for " + Quote(temporary));
        }
        CommitTemporary(temporary, outputPath, options.force);
    } catch (...) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        throw;
    }
}

} // namespace

namespace {

struct FclSegment {
    std::uint32_t offset = 0;
    std::uint32_t storedSize = 0;
};

struct Fcl {
    fs::path path;
    std::uint32_t segmentSize = 0;
    std::vector<FclSegment> segments;
    std::uint64_t fileSize = 0;
};

std::vector<std::uint8_t> ReadRange(const fs::path& path,
                                    std::uint64_t offset,
                                    std::size_t size) {
    BinaryReader reader(path);
    if (CheckedAdd(offset, size, "read range") > reader.size()) {
        Fail("range is outside " + Quote(path));
    }
    reader.seek(offset);
    return reader.readBytes(size);
}

Fcl ReadFcl(const fs::path& path) {
    BinaryReader reader(path);
    if (reader.size() < 8) {
        Fail("FCL header is truncated in " + Quote(path));
    }
    Fcl fcl;
    fcl.path = path;
    const auto count = reader.u32();
    fcl.segmentSize = reader.u32();
    fcl.fileSize = reader.size();
    if (fcl.segmentSize != kLinearSegmentSize) {
        Fail("unsupported FCL segment size 0x" + BinaryReader::Hex(fcl.segmentSize) +
             " (this runtime hardcodes 0x10000)");
    }
    if (count > kMaximumRecordCount ||
        CheckedAdd(8, static_cast<std::uint64_t>(count) * 8, "FCL table") > reader.size()) {
        Fail("invalid FCL segment count");
    }
    fcl.segments.reserve(count);
    for (std::uint32_t index = 0; index != count; ++index) {
        FclSegment segment;
        segment.offset = reader.u32();
        segment.storedSize = reader.u32();
        fcl.segments.push_back(segment);
    }
    const auto tableEnd = CheckedAdd(8, static_cast<std::uint64_t>(count) * 8,
                                     "FCL table end");
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    for (const auto& segment : fcl.segments) {
        const auto end = CheckedAdd(segment.offset, segment.storedSize,
                                    "FCL stored segment");
        if (segment.offset < tableEnd || end > fcl.fileSize || segment.storedSize == 0) {
            Fail("FCL segment is empty, overlaps its table, or exceeds EOF");
        }
        ranges.emplace_back(segment.offset, end);
    }
    std::sort(ranges.begin(), ranges.end());
    for (std::size_t index = 1; index < ranges.size(); ++index) {
        if (ranges[index].first < ranges[index - 1].second) {
            Fail("overlapping FCL segment ranges");
        }
    }
    return fcl;
}

std::vector<std::uint8_t> InflateSegment(const Fcl& fcl, std::size_t index) {
    if (index >= fcl.segments.size()) {
        Fail("FCL logical read addresses a missing segment");
    }
    const auto& segment = fcl.segments[index];
    auto stored = ReadRange(fcl.path, segment.offset, segment.storedSize);
    if (segment.storedSize == kLinearSegmentSize) {
        return stored;
    }
    if (stored.size() < 5) {
        Fail("compressed FCL segment is too short");
    }
    const auto trailerAt = stored.size() - 4;
    const std::uint32_t expected = static_cast<std::uint32_t>(stored[trailerAt]) |
                                   (static_cast<std::uint32_t>(stored[trailerAt + 1]) << 8) |
                                   (static_cast<std::uint32_t>(stored[trailerAt + 2]) << 16) |
                                   (static_cast<std::uint32_t>(stored[trailerAt + 3]) << 24);
    if (expected == 0 || expected > kLinearSegmentSize) {
        Fail("invalid FCL decompressed-size trailer");
    }
    std::vector<std::uint8_t> result(expected);
    uLongf outputSize = static_cast<uLongf>(result.size());
    const auto status = uncompress(result.data(), &outputSize,
                                   stored.data(), static_cast<uLong>(trailerAt));
    if (status != Z_OK || outputSize != result.size()) {
        Fail("zlib failed to decode FCL segment " + std::to_string(index) +
             " (status " + std::to_string(status) + ")");
    }
    return result;
}

std::uint64_t ExpandedFclSize(const Fcl& fcl) {
    if (fcl.segments.empty()) {
        return 0;
    }
    const auto last = InflateSegment(fcl, fcl.segments.size() - 1);
    return CheckedAdd(static_cast<std::uint64_t>(fcl.segments.size() - 1) *
                          kLinearSegmentSize,
                      last.size(), "expanded FCL size");
}

void ReadFclLogical(const Fcl& fcl,
                    std::uint64_t offset,
                    std::uint64_t size,
                    const std::function<void(const std::uint8_t*, std::size_t)>& sink) {
    const auto end = CheckedAdd(offset, size, "FCL logical range");
    const auto expandedSize = ExpandedFclSize(fcl);
    if (end > expandedSize) {
        Fail("FCL logical read exceeds the expanded stream");
    }
    while (size != 0) {
        const auto segmentIndex = static_cast<std::size_t>(offset / kLinearSegmentSize);
        const auto within = static_cast<std::size_t>(offset % kLinearSegmentSize);
        const auto segment = InflateSegment(fcl, segmentIndex);
        if (within >= segment.size()) {
            Fail("FCL logical offset exceeds a short segment");
        }
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(
            size, segment.size() - within));
        sink(segment.data() + within, count);
        offset += count;
        size -= count;
    }
}

std::vector<std::uint8_t> ReadFclLogicalBytes(const Fcl& fcl,
                                              std::uint64_t offset,
                                              std::uint64_t size) {
    if (size > std::numeric_limits<std::size_t>::max()) {
        Fail("logical file is too large for an in-memory edit");
    }
    std::vector<std::uint8_t> result;
    result.reserve(static_cast<std::size_t>(size));
    ReadFclLogical(fcl, offset, size,
                   [&](const std::uint8_t* data, std::size_t count) {
                       result.insert(result.end(), data, data + count);
                   });
    return result;
}

struct LhdEntry {
    std::uint32_t directoryString = 0;
    std::uint32_t filenameString = 0;
    std::uint32_t size = 0;
    std::int32_t defaultLinearIndex = -1;
};

struct LhdLinear {
    std::uint32_t nameString = 0;
    std::vector<std::uint32_t> entryIndices;
    std::vector<std::uint32_t> logicalOffsets;
};

struct LhdDirectory {
    std::uint32_t pathString = 0;
    std::vector<std::uint32_t> entryIndices;
};

struct Lhd {
    fs::path path;
    std::vector<std::string> strings;
    std::vector<LhdEntry> entries;
    std::vector<LhdLinear> linearFiles;
    std::vector<LhdDirectory> directories;
};

std::uint32_t ReadCount(BinaryReader& reader, const char* description) {
    const auto count = reader.u32();
    if (count > kMaximumRecordCount) {
        Fail(std::string("unreasonable ") + description + " count");
    }
    return count;
}

Lhd ReadLhd(const fs::path& path) {
    BinaryReader reader(path);
    Lhd lhd;
    lhd.path = path;
    const auto stringCount = ReadCount(reader, "LHD string");
    lhd.strings.reserve(stringCount);
    for (std::uint32_t index = 0; index != stringCount; ++index) {
        lhd.strings.push_back(reader.cstring());
    }

    const auto entryCount = ReadCount(reader, "LHD entry");
    lhd.entries.reserve(entryCount);
    for (std::uint32_t index = 0; index != entryCount; ++index) {
        LhdEntry entry;
        entry.directoryString = reader.u32();
        entry.filenameString = reader.u32();
        entry.size = reader.u32();
        entry.defaultLinearIndex = reader.i32();
        if (entry.directoryString >= lhd.strings.size() ||
            entry.filenameString >= lhd.strings.size()) {
            Fail("LHD entry contains an invalid string index");
        }
        lhd.entries.push_back(entry);
    }

    const auto linearCount = ReadCount(reader, "LHD linear file");
    lhd.linearFiles.reserve(linearCount);
    for (std::uint32_t linearIndex = 0; linearIndex != linearCount; ++linearIndex) {
        LhdLinear linear;
        linear.nameString = reader.u32();
        if (linear.nameString >= lhd.strings.size()) {
            Fail("LHD linear file contains an invalid name index");
        }
        const auto occurrenceCount = ReadCount(reader, "LHD occurrence");
        linear.entryIndices.reserve(occurrenceCount);
        linear.logicalOffsets.reserve(occurrenceCount);
        for (std::uint32_t index = 0; index != occurrenceCount; ++index) {
            const auto entry = reader.u32();
            if (entry >= lhd.entries.size()) {
                Fail("LHD occurrence contains an invalid entry index");
            }
            if (lhd.entries[entry].defaultLinearIndex < 0) {
                Fail("LHD occurrence references a directory entry");
            }
            linear.entryIndices.push_back(entry);
        }
        for (std::uint32_t index = 0; index != occurrenceCount; ++index) {
            linear.logicalOffsets.push_back(reader.u32());
        }
        lhd.linearFiles.push_back(std::move(linear));
    }

    const auto directoryCount = ReadCount(reader, "LHD directory");
    lhd.directories.reserve(directoryCount);
    std::vector<std::uint32_t> directoryReferences(lhd.entries.size());
    for (std::uint32_t directoryIndex = 0; directoryIndex != directoryCount;
         ++directoryIndex) {
        LhdDirectory directory;
        directory.pathString = reader.u32();
        if (directory.pathString >= lhd.strings.size()) {
            Fail("LHD directory contains an invalid string index");
        }
        const auto memberCount = ReadCount(reader, "LHD directory member");
        directory.entryIndices.reserve(memberCount);
        for (std::uint32_t index = 0; index != memberCount; ++index) {
            const auto entry = reader.u32();
            if (entry >= lhd.entries.size()) {
                Fail("LHD directory contains an invalid entry index");
            }
            ++directoryReferences[entry];
            directory.entryIndices.push_back(entry);
        }
        lhd.directories.push_back(std::move(directory));
    }
    if (reader.tell() != reader.size()) {
        Fail("unexpected trailing bytes in LHD descriptor");
    }
    for (std::size_t index = 0; index != directoryReferences.size(); ++index) {
        if (directoryReferences[index] != 1) {
            Fail("LHD entry " + std::to_string(index) +
                 " is not referenced exactly once by the directory table");
        }
    }
    return lhd;
}

std::string LogicalPath(const Lhd& lhd, std::uint32_t entryIndex) {
    if (entryIndex >= lhd.entries.size()) {
        Fail("invalid LHD entry index");
    }
    const auto& entry = lhd.entries[entryIndex];
    auto directory = lhd.strings[entry.directoryString];
    auto filename = lhd.strings[entry.filenameString];
    while (!directory.empty() &&
           (directory.back() == '/' || directory.back() == '\\')) {
        directory.pop_back();
    }
    const auto result = directory.empty() ? filename : directory + "\\" + filename;
    (void)SplitLogicalPath(result);
    return result;
}

std::string LinearName(const Lhd& lhd, std::size_t index) {
    return lhd.strings.at(lhd.linearFiles.at(index).nameString);
}

fs::path FclPathFor(const Lhd& lhd, std::size_t index) {
    return lhd.path.parent_path() / fs::u8path(LinearName(lhd, index) + ".fcl");
}

struct DescriptorMatch {
    Lhd lhd;
    std::size_t linearIndex = 0;
};

std::unique_ptr<DescriptorMatch> FindDescriptorForFcl(const fs::path& fclPath) {
    const auto stem = LowerAscii(fclPath.stem().u8string());
    std::unique_ptr<DescriptorMatch> result;
    const auto directory = fclPath.parent_path().empty() ? fs::path(".")
                                                         : fclPath.parent_path();
    for (const auto& item : fs::directory_iterator(directory)) {
        if (!fs::is_regular_file(item.path()) || ExtensionLower(item.path()) != ".lhd") {
            continue;
        }
        Lhd candidate;
        try {
            candidate = ReadLhd(item.path());
        } catch (const std::exception&) {
            continue;
        }
        for (std::size_t index = 0; index != candidate.linearFiles.size(); ++index) {
            if (LowerAscii(LinearName(candidate, index)) != stem) {
                continue;
            }
            if (result) {
                Fail("multiple LHD descriptors match " + Quote(fclPath));
            }
            result.reset(new DescriptorMatch());
            result->lhd = std::move(candidate);
            result->linearIndex = index;
            break;
        }
    }
    return result;
}

struct EntryLocation {
    std::size_t linearIndex = 0;
    std::size_t occurrence = 0;
};

std::vector<std::unique_ptr<EntryLocation>> FirstEntryLocations(const Lhd& lhd) {
    std::vector<std::unique_ptr<EntryLocation>> locations(lhd.entries.size());
    for (std::size_t linearIndex = 0; linearIndex != lhd.linearFiles.size();
         ++linearIndex) {
        const auto& linear = lhd.linearFiles[linearIndex];
        for (std::size_t occurrence = 0; occurrence != linear.entryIndices.size();
             ++occurrence) {
            const auto entry = linear.entryIndices[occurrence];
            if (!locations[entry]) {
                locations[entry].reset(new EntryLocation());
                locations[entry]->linearIndex = linearIndex;
                locations[entry]->occurrence = occurrence;
            }
        }
    }
    return locations;
}

void AppendLogicalRange(std::vector<std::uint8_t>& output,
                        const Fcl& fcl,
                        std::uint64_t offset,
                        std::uint64_t size) {
    if (size > std::numeric_limits<std::size_t>::max() - output.size()) {
        Fail("expanded FCL stream is too large for the repacker");
    }
    ReadFclLogical(fcl, offset, size,
                   [&](const std::uint8_t* data, std::size_t count) {
                       output.insert(output.end(), data, data + count);
                   });
}

std::vector<std::vector<std::uint8_t>> CompressFclBlocks(
    const std::vector<std::uint8_t>& expandedInput) {
    std::vector<std::uint8_t> expanded(expandedInput.begin(), expandedInput.end());
    if (expanded.empty()) {
        return {};
    }
    const auto padded = static_cast<std::size_t>(
        ((expanded.size() + kLinearSegmentSize - 1) / kLinearSegmentSize) *
        kLinearSegmentSize);
    expanded.resize(padded, 0);
    std::vector<std::vector<std::uint8_t>> blocks;
    blocks.reserve(expanded.size() / kLinearSegmentSize);
    for (std::size_t offset = 0; offset != expanded.size();
         offset += kLinearSegmentSize) {
        const auto* source = expanded.data() + offset;
        uLongf bound = compressBound(kLinearSegmentSize);
        std::vector<std::uint8_t> compressed(static_cast<std::size_t>(bound) + 4);
        auto compressedSize = bound;
        const auto status = compress2(compressed.data(), &compressedSize, source,
                                      kLinearSegmentSize, Z_DEFAULT_COMPRESSION);
        if (status != Z_OK) {
            Fail("zlib failed while writing FCL (status " +
                 std::to_string(status) + ")");
        }
        if (compressedSize + 4 < kLinearSegmentSize) {
            compressed.resize(static_cast<std::size_t>(compressedSize));
            AppendU32(compressed, kLinearSegmentSize);
            blocks.push_back(std::move(compressed));
        } else {
            blocks.emplace_back(source, source + kLinearSegmentSize);
        }
    }
    return blocks;
}

void WriteFcl(const std::vector<std::uint8_t>& expandedInput,
              const fs::path& outputPath,
              const Options& options) {
    const auto blocks = CompressFclBlocks(expandedInput);
    const auto tableSize = CheckedAdd(8, blocks.size() * 8, "FCL output table");
    if (blocks.size() > std::numeric_limits<std::uint32_t>::max() ||
        tableSize > std::numeric_limits<std::uint32_t>::max()) {
        Fail("FCL output exceeds 32-bit format fields");
    }
    std::vector<std::uint32_t> offsets;
    offsets.reserve(blocks.size());
    auto cursor = tableSize;
    for (const auto& block : blocks) {
        if (cursor > std::numeric_limits<std::uint32_t>::max() ||
            block.size() > std::numeric_limits<std::uint32_t>::max()) {
            Fail("FCL output exceeds 32-bit format fields");
        }
        offsets.push_back(static_cast<std::uint32_t>(cursor));
        cursor = CheckedAdd(cursor, block.size(), "FCL output data");
    }

    EnsureOutputParent(outputPath);
    const auto temporary = TemporarySibling(outputPath);
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            Fail("cannot create " + Quote(temporary));
        }
        WriteU32(output, static_cast<std::uint32_t>(blocks.size()));
        WriteU32(output, kLinearSegmentSize);
        for (std::size_t index = 0; index != blocks.size(); ++index) {
            WriteU32(output, offsets[index]);
            WriteU32(output, static_cast<std::uint32_t>(blocks[index].size()));
        }
        for (const auto& block : blocks) {
            WriteBytes(output, block);
        }
        output.close();
        if (!output) {
            Fail("write failed for " + Quote(temporary));
        }
        CommitTemporary(temporary, outputPath, options.force);
    } catch (...) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        throw;
    }
}

void WriteLhd(const Lhd& lhd,
              const fs::path& outputPath,
              const Options& options) {
    EnsureOutputParent(outputPath);
    const auto temporary = TemporarySibling(outputPath);
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            Fail("cannot create " + Quote(temporary));
        }
        WriteU32(output, static_cast<std::uint32_t>(lhd.strings.size()));
        for (const auto& string : lhd.strings) {
            output.write(string.data(), static_cast<std::streamsize>(string.size()));
            output.put('\0');
        }
        WriteU32(output, static_cast<std::uint32_t>(lhd.entries.size()));
        for (const auto& entry : lhd.entries) {
            WriteU32(output, entry.directoryString);
            WriteU32(output, entry.filenameString);
            WriteU32(output, entry.size);
            WriteU32(output, static_cast<std::uint32_t>(entry.defaultLinearIndex));
        }
        WriteU32(output, static_cast<std::uint32_t>(lhd.linearFiles.size()));
        for (const auto& linear : lhd.linearFiles) {
            WriteU32(output, linear.nameString);
            WriteU32(output, static_cast<std::uint32_t>(linear.entryIndices.size()));
            for (const auto entry : linear.entryIndices) {
                WriteU32(output, entry);
            }
            for (const auto offset : linear.logicalOffsets) {
                WriteU32(output, offset);
            }
        }
        WriteU32(output, static_cast<std::uint32_t>(lhd.directories.size()));
        for (const auto& directory : lhd.directories) {
            WriteU32(output, directory.pathString);
            WriteU32(output, static_cast<std::uint32_t>(directory.entryIndices.size()));
            for (const auto entry : directory.entryIndices) {
                WriteU32(output, entry);
            }
        }
        output.close();
        if (!output) {
            Fail("write failed for " + Quote(temporary));
        }
        CommitTemporary(temporary, outputPath, options.force);
    } catch (...) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        throw;
    }
}

void CopyFileTransactional(const fs::path& source,
                           const fs::path& outputPath,
                           const Options& options) {
    BinaryReader reader(source);
    EnsureOutputParent(outputPath);
    const auto temporary = TemporarySibling(outputPath);
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            Fail("cannot create " + Quote(temporary));
        }
        CopyRange(source, 0, reader.size(), output);
        output.close();
        if (!output) {
            Fail("write failed for " + Quote(temporary));
        }
        CommitTemporary(temporary, outputPath, options.force);
    } catch (...) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        throw;
    }
}

std::map<std::uint32_t, fs::path> FindModifiedFiles(const Lhd& lhd,
                                                    const fs::path& directory) {
    if (!fs::is_directory(directory)) {
        Fail("modified input is not a directory: " + Quote(directory));
    }
    std::map<std::string, std::uint32_t> expected;
    for (std::uint32_t index = 0; index != lhd.entries.size(); ++index) {
        if (lhd.entries[index].defaultLinearIndex >= 0) {
            expected.emplace(LowerAscii(LogicalPath(lhd, index)), index);
        }
    }
    std::map<std::uint32_t, fs::path> result;
    for (const auto& item : fs::recursive_directory_iterator(directory)) {
        if (fs::is_directory(item.path())) {
            continue;
        }
        if (!fs::is_regular_file(item.path())) {
            Fail("refusing non-regular modified object " + Quote(item.path()));
        }
        auto relative = RelativeContainedPath(item.path(), directory);
        std::replace(relative.begin(), relative.end(), '/', '\\');
        const auto found = expected.find(LowerAscii(relative));
        if (found == expected.end()) {
            Fail("modified directory contains a file absent from the LHD: '" +
                 relative + "'");
        }
        result[found->second] = item.path();
    }
    return result;
}

std::vector<std::uint8_t> ReadDiskFile(const fs::path& path) {
    BinaryReader reader(path);
    if (reader.size() > std::numeric_limits<std::size_t>::max()) {
        Fail("file is too large for an in-memory FCL rebuild: " + Quote(path));
    }
    return reader.readBytes(static_cast<std::size_t>(reader.size()));
}

void RepackLhdPackage(const fs::path& originalPath,
                      const fs::path& modifiedDirectory,
                      const fs::path& outputPath,
                      const Options& options) {
    const auto originalLhd = ReadLhd(originalPath);
    auto lhd = originalLhd;
    const auto modified = FindModifiedFiles(lhd, modifiedDirectory);
    for (auto item = modified.begin(); item != modified.end(); ++item) {
        const auto entryIndex = item->first;
        const auto& path = item->second;
        const auto size = fs::file_size(path);
        if (size > std::numeric_limits<std::uint32_t>::max()) {
            Fail("LHD logical file exceeds 4 GiB: " + Quote(path));
        }
        lhd.entries[entryIndex].size = static_cast<std::uint32_t>(size);
    }

    const auto outputDirectory = outputPath.parent_path().empty()
                                     ? fs::path(".")
                                     : outputPath.parent_path();
    for (std::size_t linearIndex = 0; linearIndex != lhd.linearFiles.size();
         ++linearIndex) {
        auto& linear = lhd.linearFiles[linearIndex];
        const auto originalFclPath = originalPath.parent_path() /
                                     fs::u8path(LinearName(lhd, linearIndex) + ".fcl");
        const auto outputFclPath = outputDirectory /
                                   fs::u8path(LinearName(lhd, linearIndex) + ".fcl");
        const auto originalFcl = ReadFcl(originalFclPath);
        if (linear.entryIndices.empty()) {
            if (ComparableAbsolute(originalFclPath) ==
                ComparableAbsolute(outputFclPath)) {
                continue;
            }
            CopyFileTransactional(originalFclPath, outputFclPath, options);
            continue;
        }

        std::vector<std::uint8_t> expanded;
        std::uint64_t oldCursor = 0;
        for (std::size_t occurrence = 0; occurrence != linear.entryIndices.size();
             ++occurrence) {
            const auto entryIndex = linear.entryIndices[occurrence];
            const auto oldOffset = linear.logicalOffsets[occurrence];
            const auto sourceSize = originalLhd.entries[entryIndex].size;
            if (oldOffset < oldCursor) {
                Fail("size-changing LHD repack requires monotonic occurrence offsets");
            }
            AppendLogicalRange(expanded, originalFcl, oldCursor, oldOffset - oldCursor);
            if (expanded.size() > std::numeric_limits<std::uint32_t>::max()) {
                Fail("LHD logical offset exceeds 32-bit format field");
            }
            linear.logicalOffsets[occurrence] = static_cast<std::uint32_t>(expanded.size());
            const auto replacement = modified.find(entryIndex);
            if (replacement != modified.end()) {
                const auto bytes = ReadDiskFile(replacement->second);
                expanded.insert(expanded.end(), bytes.begin(), bytes.end());
            } else {
                AppendLogicalRange(expanded, originalFcl, oldOffset, sourceSize);
            }
            oldCursor = CheckedAdd(oldOffset, sourceSize, "old LHD occurrence end");
        }
        const auto originalExpandedSize = ExpandedFclSize(originalFcl);
        if (oldCursor > originalExpandedSize) {
            Fail("LHD occurrences exceed their original FCL stream");
        }
        AppendLogicalRange(expanded, originalFcl, oldCursor,
                           originalExpandedSize - oldCursor);
        WriteFcl(expanded, outputFclPath, options);
        if (options.verbose) {
            std::cout << "rebuilt " << outputFclPath.u8string() << " ("
                      << linear.entryIndices.size() << " occurrences)\n";
        }
    }
    WriteLhd(lhd, outputPath, options);
}

void RepackSingleFcl(const fs::path& originalPath,
                     const fs::path& modifiedDirectory,
                     const fs::path& outputPath,
                     const Options& options) {
    const auto match = FindDescriptorForFcl(originalPath);
    if (!match) {
        Fail("cannot logically repack an FCL without a matching adjacent LHD");
    }
    const auto modified = FindModifiedFiles(match->lhd, modifiedDirectory);
    const auto fcl = ReadFcl(originalPath);
    const auto expandedSize = ExpandedFclSize(fcl);
    if (expandedSize > std::numeric_limits<std::size_t>::max()) {
        Fail("FCL is too large for an in-memory rebuild");
    }
    std::vector<std::uint8_t> expanded;
    expanded.reserve(static_cast<std::size_t>(expandedSize));
    AppendLogicalRange(expanded, fcl, 0, expandedSize);
    const auto& linear = match->lhd.linearFiles[match->linearIndex];
    for (std::size_t occurrence = 0; occurrence != linear.entryIndices.size();
         ++occurrence) {
        const auto entryIndex = linear.entryIndices[occurrence];
        const auto replacement = modified.find(entryIndex);
        if (replacement == modified.end()) {
            continue;
        }
        const auto bytes = ReadDiskFile(replacement->second);
        const auto expected = match->lhd.entries[entryIndex].size;
        if (bytes.size() != expected) {
            Fail("single-FCL repack cannot change logical sizes; repack the .lhd package instead");
        }
        const auto offset = linear.logicalOffsets[occurrence];
        if (CheckedAdd(offset, bytes.size(), "FCL replacement") > expanded.size()) {
            Fail("FCL replacement range exceeds expanded stream");
        }
        std::copy(bytes.begin(), bytes.end(), expanded.begin() + offset);
    }
    WriteFcl(expanded, outputPath, options);
}

} // namespace

Format DetectFormat(const fs::path& path) {
    const auto extension = ExtensionLower(path);
    if (extension == ".bundle") {
        return Format::bundle;
    }
    if (extension == ".fcl") {
        return Format::fcl;
    }
    if (extension == ".lhd") {
        return Format::lhd;
    }
    BinaryReader reader(path);
    if (reader.size() >= 4) {
        const auto tag = reader.readBytes(4);
        if (std::equal(tag.begin(), tag.end(), "BNDL")) {
            return Format::bundle;
        }
    }
    Fail("cannot detect archive format from extension or magic: " + Quote(path));
}

namespace {

void PrintBundleInfo(const fs::path& path) {
    const auto bundle = ReadBundle(path);
    const auto stats = GetBundleStats(bundle);
    std::cout << "format: BNDL\n"
              << "version: " << bundle.version << "\n"
              << "file_size: " << bundle.fileSize << "\n"
              << "index_end: 0x" << std::hex << std::uppercase << bundle.indexEnd
              << std::dec << " (" << bundle.indexEnd << ")\n"
              << "reserved: 0x" << std::hex << std::uppercase
              << static_cast<std::uint32_t>(bundle.reserved) << std::dec << "\n"
              << "directories: " << stats.directories << "\n"
              << "files: " << stats.files << "\n"
              << "zero_size_files: " << stats.zeroSizeFiles << "\n"
              << "payload_bytes: " << stats.storedBytes << "\n"
              << "compression: none (direct payload ranges)\n";
}

void PrintBundleList(const fs::path& path, const Options& options) {
    const auto bundle = ReadBundle(path);
    std::cout << "type\texists\toffset\tstored\tunpacked\tcompression\tpath\n";
    VisitBundle(bundle.root, "", [&](const BundleNode& node, const std::string& logical) {
        if (node.kind == BundleNodeKind::directory) {
            if (options.verbose) {
                std::cout << "dir\t" << (node.exists ? 1 : 0)
                          << "\t-\t-\t-\t-\t" << logical << "\n";
            }
            return;
        }
        std::cout << "file\t" << (node.exists ? 1 : 0) << "\t0x"
                  << std::hex << std::uppercase << node.offset << std::dec << "\t"
                  << node.size << "\t" << node.size << "\tnone\t" << logical
                  << "\n";
    });
}

void PrepareExtractionRoot(const fs::path& outputDirectory) {
    if (fs::exists(outputDirectory) && !fs::is_directory(outputDirectory)) {
        Fail("extraction output is not a directory: " + Quote(outputDirectory));
    }
    std::error_code error;
    fs::create_directories(outputDirectory, error);
    if (error) {
        Fail("cannot create extraction directory " + Quote(outputDirectory) +
             ": " + error.message());
    }
}

void ExtractBundle(const fs::path& path,
                   const fs::path& outputDirectory,
                   const Options& options) {
    const auto bundle = ReadBundle(path);
    PrepareExtractionRoot(outputDirectory);
    std::uint64_t extracted = 0;
    std::uint64_t skipped = 0;
    VisitBundle(bundle.root, "", [&](const BundleNode& node, const std::string& logical) {
        if (node.kind == BundleNodeKind::directory) {
            if (node.exists) {
                std::error_code error;
                fs::create_directories(SafeOutputPath(outputDirectory, logical), error);
                if (error) {
                    Fail("cannot create output directory for '" + logical + "'");
                }
            }
            return;
        }
        if (!node.exists) {
            ++skipped;
            if (options.verbose) {
                std::cout << "skip tombstone " << logical << "\n";
            }
            return;
        }
        const auto destination = SafeOutputPath(outputDirectory, logical);
        WriteExtractedFile(destination, options, [&](std::ostream& output) {
            CopyRange(path, node.offset, node.size, output);
        });
        ++extracted;
        if (options.verbose) {
            std::cout << "extract " << logical << " (" << node.size << " bytes)\n";
        }
    });
    std::cout << "extracted " << extracted << " BNDL files";
    if (skipped != 0) {
        std::cout << "; skipped " << skipped << " non-existing entries";
    }
    std::cout << "\n";
}

struct FclStats {
    std::uint64_t storedBytes = 0;
    std::uint64_t expandedBytes = 0;
    std::uint64_t compressed = 0;
    std::uint64_t raw = 0;
};

FclStats GetFclStats(const Fcl& fcl) {
    FclStats stats;
    for (std::size_t index = 0; index != fcl.segments.size(); ++index) {
        const auto& segment = fcl.segments[index];
        const auto expanded = InflateSegment(fcl, index);
        stats.storedBytes += segment.storedSize;
        stats.expandedBytes += expanded.size();
        if (segment.storedSize == kLinearSegmentSize) {
            ++stats.raw;
        } else {
            ++stats.compressed;
        }
    }
    return stats;
}

void PrintFclInfo(const fs::path& path) {
    const auto fcl = ReadFcl(path);
    const auto stats = GetFclStats(fcl);
    const auto descriptor = FindDescriptorForFcl(path);
    std::cout << "format: FCL segmented linear data\n"
              << "file_size: " << fcl.fileSize << "\n"
              << "segments: " << fcl.segments.size() << "\n"
              << "segment_size: 0x" << std::hex << std::uppercase
              << fcl.segmentSize << std::dec << "\n"
              << "stored_bytes: " << stats.storedBytes << "\n"
              << "expanded_bytes: " << stats.expandedBytes << "\n"
              << "zlib_segments: " << stats.compressed << "\n"
              << "raw_segments: " << stats.raw << "\n";
    if (descriptor) {
        const auto& linear = descriptor->lhd.linearFiles[descriptor->linearIndex];
        std::cout << "descriptor: " << descriptor->lhd.path.u8string() << "\n"
                  << "linear_name: "
                  << LinearName(descriptor->lhd, descriptor->linearIndex) << "\n"
                  << "logical_occurrences: " << linear.entryIndices.size() << "\n";
    } else {
        std::cout << "descriptor: not found (logical filenames unavailable)\n";
    }
}

void PrintFclSegments(const Fcl& fcl) {
    std::cout << "segment\toffset\tstored\tunpacked\tcompression\n";
    for (std::size_t index = 0; index != fcl.segments.size(); ++index) {
        const auto expanded = InflateSegment(fcl, index);
        const auto& segment = fcl.segments[index];
        std::cout << index << "\t0x" << std::hex << std::uppercase
                  << segment.offset << std::dec << "\t" << segment.storedSize
                  << "\t" << expanded.size() << "\t"
                  << (segment.storedSize == kLinearSegmentSize ? "raw" : "zlib+rsize")
                  << "\n";
    }
}

void PrintFclList(const fs::path& path, const Options& options) {
    const auto fcl = ReadFcl(path);
    const auto descriptor = FindDescriptorForFcl(path);
    if (!descriptor) {
        PrintFclSegments(fcl);
        return;
    }
    const auto& lhd = descriptor->lhd;
    const auto& linear = lhd.linearFiles[descriptor->linearIndex];
    std::cout << "occurrence\tentry\toffset\tstored\tunpacked\tcompression\tpath\n";
    for (std::size_t occurrence = 0; occurrence != linear.entryIndices.size();
         ++occurrence) {
        const auto entryIndex = linear.entryIndices[occurrence];
        const auto& entry = lhd.entries[entryIndex];
        const auto logical = LinearName(lhd, descriptor->linearIndex) + ":" +
                             LogicalPath(lhd, entryIndex);
        std::cout << occurrence << "\t" << entryIndex << "\t0x" << std::hex
                  << std::uppercase << linear.logicalOffsets[occurrence] << std::dec
                  << "\t-\t" << entry.size << "\tsegmented\t" << logical << "\n";
    }
    if (options.verbose) {
        std::cout << "\n# Physical segment table\n";
        PrintFclSegments(fcl);
    }
}

void PrintLhdInfo(const fs::path& path) {
    const auto lhd = ReadLhd(path);
    std::uint64_t files = 0;
    std::uint64_t directoryEntries = 0;
    std::uint64_t occurrences = 0;
    for (const auto& entry : lhd.entries) {
        if (entry.defaultLinearIndex < 0) {
            ++directoryEntries;
        } else {
            ++files;
        }
    }
    for (const auto& linear : lhd.linearFiles) {
        occurrences += linear.entryIndices.size();
    }
    std::cout << "format: LHD linear package descriptor\n"
              << "file_size: " << fs::file_size(path) << "\n"
              << "strings: " << lhd.strings.size() << "\n"
              << "entries: " << lhd.entries.size() << "\n"
              << "files: " << files << "\n"
              << "directory_entries: " << directoryEntries << "\n"
              << "linear_files: " << lhd.linearFiles.size() << "\n"
              << "occurrences: " << occurrences << "\n"
              << "directory_records: " << lhd.directories.size() << "\n"
              << "linear_names:";
    for (std::size_t index = 0; index != lhd.linearFiles.size(); ++index) {
        std::cout << (index == 0 ? " " : ", ") << LinearName(lhd, index);
    }
    std::cout << "\n";
}

void PrintLhdList(const fs::path& path, const Options& options) {
    const auto lhd = ReadLhd(path);
    const auto locations = FirstEntryLocations(lhd);
    std::vector<std::uint32_t> occurrenceCounts(lhd.entries.size());
    for (const auto& linear : lhd.linearFiles) {
        for (const auto entry : linear.entryIndices) {
            ++occurrenceCounts[entry];
        }
    }
    std::cout << "entry\tdefault\toccurrences\tlinear\toffset\tsize\tpath\n";
    for (std::uint32_t index = 0; index != lhd.entries.size(); ++index) {
        const auto& entry = lhd.entries[index];
        if (entry.defaultLinearIndex < 0 && !options.verbose) {
            continue;
        }
        std::cout << index << "\t" << entry.defaultLinearIndex << "\t"
                  << occurrenceCounts[index] << "\t";
        if (locations[index]) {
            const auto& location = *locations[index];
            std::cout << LinearName(lhd, location.linearIndex) << "\t0x" << std::hex
                      << std::uppercase
                      << lhd.linearFiles[location.linearIndex]
                             .logicalOffsets[location.occurrence]
                      << std::dec;
        } else {
            std::cout << "-\t-";
        }
        std::cout << "\t" << entry.size << "\t" << LogicalPath(lhd, index)
                  << "\n";
    }
}

void ExtractLogicalEntry(const Lhd& lhd,
                         const Fcl& fcl,
                         std::uint32_t entryIndex,
                         std::uint32_t logicalOffset,
                         const fs::path& outputDirectory,
                         const Options& options) {
    const auto logical = LogicalPath(lhd, entryIndex);
    const auto& entry = lhd.entries[entryIndex];
    const auto destination = SafeOutputPath(outputDirectory, logical);
    WriteExtractedFile(destination, options, [&](std::ostream& output) {
        ReadFclLogical(fcl, logicalOffset, entry.size,
                       [&](const std::uint8_t* data, std::size_t count) {
                           output.write(reinterpret_cast<const char*>(data),
                                        static_cast<std::streamsize>(count));
                           if (!output) {
                               Fail("write failed for " + Quote(destination));
                           }
                       });
    });
    if (options.verbose) {
        std::cout << "extract " << logical << " (" << entry.size << " bytes)\n";
    }
}

void ExtractFcl(const fs::path& path,
                const fs::path& outputDirectory,
                const Options& options) {
    const auto fcl = ReadFcl(path);
    PrepareExtractionRoot(outputDirectory);
    const auto descriptor = FindDescriptorForFcl(path);
    if (!descriptor) {
        const auto destination = outputDirectory / "__linear_stream.bin";
        WriteExtractedFile(destination, options, [&](std::ostream& output) {
            for (std::size_t index = 0; index != fcl.segments.size(); ++index) {
                const auto segment = InflateSegment(fcl, index);
                WriteBytes(output, segment);
            }
        });
        std::cout << "no adjacent LHD matched; wrote expanded linear stream to "
                  << destination.u8string() << "\n";
        return;
    }
    const auto& lhd = descriptor->lhd;
    const auto& linear = lhd.linearFiles[descriptor->linearIndex];
    std::set<std::uint32_t> extracted;
    for (std::size_t occurrence = 0; occurrence != linear.entryIndices.size();
         ++occurrence) {
        const auto entry = linear.entryIndices[occurrence];
        if (!extracted.insert(entry).second) {
            continue;
        }
        ExtractLogicalEntry(lhd, fcl, entry, linear.logicalOffsets[occurrence],
                            outputDirectory, options);
    }
    std::cout << "extracted " << extracted.size() << " unique logical files from "
              << LinearName(lhd, descriptor->linearIndex) << " ("
              << linear.entryIndices.size() << " stored occurrences)\n";
}

void ExtractLhd(const fs::path& path,
                const fs::path& outputDirectory,
                const Options& options) {
    const auto lhd = ReadLhd(path);
    PrepareExtractionRoot(outputDirectory);
    const auto locations = FirstEntryLocations(lhd);
    std::map<std::size_t, Fcl> opened;
    std::uint64_t extracted = 0;
    for (std::uint32_t entryIndex = 0; entryIndex != lhd.entries.size(); ++entryIndex) {
        if (lhd.entries[entryIndex].defaultLinearIndex < 0) {
            continue;
        }
        if (!locations[entryIndex]) {
            Fail("logical LHD file has no FCL occurrence: " +
                 LogicalPath(lhd, entryIndex));
        }
        const auto location = *locations[entryIndex];
        auto found = opened.find(location.linearIndex);
        if (found == opened.end()) {
            found = opened.emplace(location.linearIndex,
                                   ReadFcl(FclPathFor(lhd, location.linearIndex)))
                        .first;
        }
        ExtractLogicalEntry(
            lhd, found->second, entryIndex,
            lhd.linearFiles[location.linearIndex].logicalOffsets[location.occurrence],
            outputDirectory, options);
        ++extracted;
    }
    std::cout << "extracted " << extracted << " logical files from LHD/FCL package\n";
}

struct BundleRecord {
    const BundleNode* node = nullptr;
    std::string logical;
};

std::map<std::string, BundleRecord> BundleRecords(const Bundle& bundle) {
    std::map<std::string, BundleRecord> result;
    VisitBundle(bundle.root, "", [&](const BundleNode& node, const std::string& logical) {
        BundleRecord record;
        record.node = &node;
        record.logical = logical;
        result.emplace(LowerAscii(logical), std::move(record));
    });
    return result;
}

bool CompareReaderRanges(BinaryReader& left,
                         std::uint64_t leftOffset,
                         BinaryReader& right,
                         std::uint64_t rightOffset,
                         std::uint64_t size) {
    left.seek(leftOffset);
    right.seek(rightOffset);
    std::vector<std::uint8_t> leftBytes(kCopyBufferSize);
    std::vector<std::uint8_t> rightBytes(kCopyBufferSize);
    while (size != 0) {
        const auto count = static_cast<std::size_t>(
            std::min<std::uint64_t>(size, kCopyBufferSize));
        left.read(leftBytes.data(), count);
        right.read(rightBytes.data(), count);
        if (!std::equal(leftBytes.begin(), leftBytes.begin() + count,
                        rightBytes.begin())) {
            return false;
        }
        size -= count;
    }
    return true;
}

bool VerifyBundle(const fs::path& originalPath,
                  const fs::path& candidatePath,
                  const Options& options) {
    const auto original = ReadBundle(originalPath);
    const auto candidate = ReadBundle(candidatePath);
    const auto left = BundleRecords(original);
    const auto right = BundleRecords(candidate);
    bool compatible = true;
    bool exact = original.version == candidate.version &&
                 original.reserved == candidate.reserved;
    std::uint64_t unchanged = 0;
    std::uint64_t changed = 0;
    std::uint64_t missing = 0;
    std::uint64_t added = 0;
    BinaryReader originalReader(originalPath);
    BinaryReader candidateReader(candidatePath);
    for (auto item = left.begin(); item != left.end(); ++item) {
        const auto& key = item->first;
        const auto& record = item->second;
        const auto found = right.find(key);
        if (found == right.end()) {
            ++missing;
            compatible = false;
            exact = false;
            if (options.verbose) {
                std::cout << "missing: " << record.logical << "\n";
            }
            continue;
        }
        const auto& leftNode = *record.node;
        const auto& rightNode = *found->second.node;
        if (leftNode.kind != rightNode.kind || leftNode.exists != rightNode.exists) {
            compatible = false;
            exact = false;
            ++changed;
            if (options.verbose) {
                std::cout << "metadata changed: " << record.logical << "\n";
            }
            continue;
        }
        if (leftNode.kind == BundleNodeKind::directory) {
            ++unchanged;
            continue;
        }
        bool same = leftNode.size == rightNode.size;
        if (same && leftNode.size != 0) {
            same = CompareReaderRanges(originalReader, leftNode.offset,
                                       candidateReader, rightNode.offset,
                                       leftNode.size);
        }
        if (same) {
            ++unchanged;
        } else {
            ++changed;
            exact = false;
            if (options.verbose) {
                std::cout << "payload changed: " << record.logical << " ("
                          << leftNode.size << " -> " << rightNode.size << ")\n";
            }
        }
    }
    for (auto item = right.begin(); item != right.end(); ++item) {
        const auto& key = item->first;
        const auto& record = item->second;
        if (left.count(key) == 0) {
            ++added;
            compatible = false;
            exact = false;
            if (options.verbose) {
                std::cout << "added: " << record.logical << "\n";
            }
        }
    }
    std::cout << "BNDL verification: unchanged=" << unchanged
              << " changed=" << changed << " missing=" << missing
              << " added=" << added << "\n";
    if (!compatible) {
        std::cout << "result: structurally valid, but path/type metadata is incompatible\n";
        return false;
    }
    if (options.strict && !exact) {
        std::cout << "result: compatible but not strictly equivalent\n";
        return false;
    }
    std::cout << "result: " << (exact ? "strictly equivalent" :
                                "compatible; modified payloads are valid") << "\n";
    return true;
}

bool VerifyFcl(const fs::path& originalPath,
               const fs::path& candidatePath,
               const Options& options) {
    const auto original = ReadFcl(originalPath);
    const auto candidate = ReadFcl(candidatePath);
    const auto originalSize = ExpandedFclSize(original);
    const auto candidateSize = ExpandedFclSize(candidate);
    bool exact = originalSize == candidateSize;
    const auto commonSegments = std::min(original.segments.size(), candidate.segments.size());
    std::uint64_t changedSegments = 0;
    for (std::size_t index = 0; index != commonSegments; ++index) {
        const auto left = InflateSegment(original, index);
        const auto right = InflateSegment(candidate, index);
        if (left != right) {
            ++changedSegments;
            exact = false;
        }
    }
    for (std::size_t index = commonSegments; index != original.segments.size(); ++index) {
        (void)InflateSegment(original, index);
        ++changedSegments;
        exact = false;
    }
    for (std::size_t index = commonSegments; index != candidate.segments.size(); ++index) {
        (void)InflateSegment(candidate, index);
        ++changedSegments;
        exact = false;
    }
    std::cout << "FCL verification: original_segments=" << original.segments.size()
              << " candidate_segments=" << candidate.segments.size()
              << " changed_segments=" << changedSegments << "\n";
    if (options.strict && !exact) {
        std::cout << "result: valid but expanded streams differ\n";
        return false;
    }
    std::cout << "result: " << (exact ? "expanded streams are equivalent" :
                                "candidate stream is valid and intentionally modified")
              << "\n";
    return true;
}

std::map<std::string, std::uint32_t> LhdFilePaths(const Lhd& lhd) {
    std::map<std::string, std::uint32_t> result;
    for (std::uint32_t index = 0; index != lhd.entries.size(); ++index) {
        if (lhd.entries[index].defaultLinearIndex >= 0) {
            result.emplace(LowerAscii(LogicalPath(lhd, index)), index);
        }
    }
    return result;
}

std::vector<std::uint8_t> ReadLhdEntry(const Lhd& lhd,
                                       std::uint32_t entryIndex,
                                       const std::vector<std::unique_ptr<EntryLocation>>& locations,
                                       std::map<std::size_t, Fcl>& fcls) {
    if (!locations.at(entryIndex)) {
        Fail("LHD file has no physical occurrence: " + LogicalPath(lhd, entryIndex));
    }
    const auto location = *locations[entryIndex];
    auto found = fcls.find(location.linearIndex);
    if (found == fcls.end()) {
        found = fcls.emplace(location.linearIndex,
                             ReadFcl(FclPathFor(lhd, location.linearIndex)))
                    .first;
    }
    return ReadFclLogicalBytes(
        found->second,
        lhd.linearFiles[location.linearIndex].logicalOffsets[location.occurrence],
        lhd.entries[entryIndex].size);
}

bool VerifyLhd(const fs::path& originalPath,
               const fs::path& candidatePath,
               const Options& options) {
    const auto original = ReadLhd(originalPath);
    const auto candidate = ReadLhd(candidatePath);
    const auto leftPaths = LhdFilePaths(original);
    const auto rightPaths = LhdFilePaths(candidate);
    const auto leftLocations = FirstEntryLocations(original);
    const auto rightLocations = FirstEntryLocations(candidate);
    std::map<std::size_t, Fcl> leftFcls;
    std::map<std::size_t, Fcl> rightFcls;
    bool compatible = true;
    bool exact = true;
    std::uint64_t unchanged = 0;
    std::uint64_t changed = 0;
    std::uint64_t missing = 0;
    std::uint64_t added = 0;
    for (auto item = leftPaths.begin(); item != leftPaths.end(); ++item) {
        const auto& key = item->first;
        const auto leftIndex = item->second;
        const auto found = rightPaths.find(key);
        if (found == rightPaths.end()) {
            ++missing;
            compatible = false;
            exact = false;
            if (options.verbose) {
                std::cout << "missing: " << LogicalPath(original, leftIndex) << "\n";
            }
            continue;
        }
        const auto rightIndex = found->second;
        bool same = original.entries[leftIndex].size == candidate.entries[rightIndex].size;
        if (same) {
            same = ReadLhdEntry(original, leftIndex, leftLocations, leftFcls) ==
                   ReadLhdEntry(candidate, rightIndex, rightLocations, rightFcls);
        } else {
            (void)ReadLhdEntry(original, leftIndex, leftLocations, leftFcls);
            (void)ReadLhdEntry(candidate, rightIndex, rightLocations, rightFcls);
        }
        if (same) {
            ++unchanged;
        } else {
            ++changed;
            exact = false;
            if (options.verbose) {
                std::cout << "payload changed: " << LogicalPath(original, leftIndex)
                          << " (" << original.entries[leftIndex].size << " -> "
                          << candidate.entries[rightIndex].size << ")\n";
            }
        }
    }
    for (auto item = rightPaths.begin(); item != rightPaths.end(); ++item) {
        const auto& key = item->first;
        const auto rightIndex = item->second;
        if (leftPaths.count(key) == 0) {
            ++added;
            compatible = false;
            exact = false;
            if (options.verbose) {
                std::cout << "added: " << LogicalPath(candidate, rightIndex) << "\n";
            }
        }
    }
    if (original.linearFiles.size() != candidate.linearFiles.size() ||
        original.directories.size() != candidate.directories.size()) {
        compatible = false;
        exact = false;
    }
    std::cout << "LHD/FCL verification: unchanged=" << unchanged
              << " changed=" << changed << " missing=" << missing
              << " added=" << added << "\n";
    if (!compatible) {
        std::cout << "result: valid package but incompatible logical structure\n";
        return false;
    }
    if (options.strict && !exact) {
        std::cout << "result: compatible package but logical payloads differ\n";
        return false;
    }
    std::cout << "result: " << (exact ? "logical payloads are equivalent" :
                                "compatible package with valid modified payloads")
              << "\n";
    return true;
}

} // namespace

void PrintInfo(const fs::path& archive, const Options&) {
    switch (DetectFormat(archive)) {
    case Format::bundle:
        PrintBundleInfo(archive);
        break;
    case Format::fcl:
        PrintFclInfo(archive);
        break;
    case Format::lhd:
        PrintLhdInfo(archive);
        break;
    }
}

void PrintList(const fs::path& archive, const Options& options) {
    switch (DetectFormat(archive)) {
    case Format::bundle:
        PrintBundleList(archive, options);
        break;
    case Format::fcl:
        PrintFclList(archive, options);
        break;
    case Format::lhd:
        PrintLhdList(archive, options);
        break;
    }
}

void Extract(const fs::path& archive,
             const fs::path& outputDirectory,
             const Options& options) {
    switch (DetectFormat(archive)) {
    case Format::bundle:
        ExtractBundle(archive, outputDirectory, options);
        break;
    case Format::fcl:
        ExtractFcl(archive, outputDirectory, options);
        break;
    case Format::lhd:
        ExtractLhd(archive, outputDirectory, options);
        break;
    }
}

void Pack(const fs::path& inputDirectory,
          const fs::path& outputArchive,
          const Options& options) {
    switch (DetectFormat(outputArchive)) {
    case Format::bundle: {
        auto root = BuildBundleDirectory(inputDirectory);
        WriteBundle(std::move(root), 0, outputArchive, options);
        std::cout << "packed BNDL " << outputArchive.u8string() << "\n";
        break;
    }
    case Format::fcl: {
        const auto streamPath = inputDirectory / "__linear_stream.bin";
        if (!fs::is_regular_file(streamPath)) {
            Fail("raw FCL pack expects <input-directory>/__linear_stream.bin");
        }
        WriteFcl(ReadDiskFile(streamPath), outputArchive, options);
        std::cout << "packed FCL " << outputArchive.u8string() << "\n";
        break;
    }
    case Format::lhd:
        Fail("creating a new LHD requires semantic placement metadata; use template repack");
    }
}

void Repack(const fs::path& originalArchive,
            const fs::path& modifiedDirectory,
            const fs::path& outputArchive,
            const Options& options) {
    const auto originalFormat = DetectFormat(originalArchive);
    const auto outputFormat = DetectFormat(outputArchive);
    if (originalFormat != outputFormat) {
        Fail("original and output archive formats differ");
    }
    switch (originalFormat) {
    case Format::bundle: {
        auto bundle = ReadBundle(originalArchive);
        SetOriginalBundleSources(bundle.root, originalArchive);
        ApplyBundleOverlay(bundle.root, modifiedDirectory, "");
        WriteBundle(std::move(bundle.root), bundle.reserved, outputArchive, options);
        std::cout << "repacked BNDL " << outputArchive.u8string() << "\n";
        break;
    }
    case Format::fcl:
        RepackSingleFcl(originalArchive, modifiedDirectory, outputArchive, options);
        std::cout << "repacked FCL " << outputArchive.u8string() << "\n";
        break;
    case Format::lhd:
        RepackLhdPackage(originalArchive, modifiedDirectory, outputArchive, options);
        std::cout << "repacked LHD/FCL package " << outputArchive.u8string() << "\n";
        break;
    }
}

bool Verify(const fs::path& originalArchive,
            const fs::path& candidateArchive,
            const Options& options) {
    const auto originalFormat = DetectFormat(originalArchive);
    const auto candidateFormat = DetectFormat(candidateArchive);
    if (originalFormat != candidateFormat) {
        Fail("cannot verify different archive formats");
    }
    switch (originalFormat) {
    case Format::bundle:
        return VerifyBundle(originalArchive, candidateArchive, options);
    case Format::fcl:
        return VerifyFcl(originalArchive, candidateArchive, options);
    case Format::lhd:
        return VerifyLhd(originalArchive, candidateArchive, options);
    }
    return false;
}

} // namespace ArchiveTool
} // namespace Scream
