#include "ScreamSDK/Build.hpp"

#include <Windows.h>
#include <wincrypt.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

namespace Scream {
namespace Build {

const GameBuild kScreamPublicDev = {
    0x014C,
    0x4D359BDE,
    0x0089C000,
    0x00578208,
    "8185066A-E591-4EFC-8690-309F57F330DD",
    1,
    "31bead0c3a982d085a64b594496448ef93bc7f12bfd47c07a892f7cab2ad80bb"
};

namespace {

class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE handle = INVALID_HANDLE_VALUE) : handle_(handle) {}
    ~ScopedHandle() {
        if (handle_ != INVALID_HANDLE_VALUE && handle_ != NULL) {
            CloseHandle(handle_);
        }
    }
    HANDLE get() const { return handle_; }
private:
    HANDLE handle_;
};

class ScopedMap {
public:
    explicit ScopedMap(const void* view = NULL) : view_(view) {}
    ~ScopedMap() {
        if (view_ != NULL) {
            UnmapViewOfFile(view_);
        }
    }
    const std::uint8_t* bytes() const {
        return static_cast<const std::uint8_t*>(view_);
    }
private:
    const void* view_;
};

bool AddFits(std::uint64_t offset, std::uint64_t size, std::uint64_t total) {
    return offset <= total && size <= total - offset;
}

std::string GuidString(const GUID& guid) {
    char text[37] = {};
#if defined(_MSC_VER)
    _snprintf_s(text, sizeof(text), _TRUNCATE,
                "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                guid.Data1, guid.Data2, guid.Data3,
                guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
#else
    snprintf(text, sizeof(text),
             "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
             guid.Data1, guid.Data2, guid.Data3,
             guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
             guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
#endif
    return text;
}

void ReadCodeViewFingerprint(const std::uint8_t* base,
                             const IMAGE_NT_HEADERS32* nt,
                             std::string& guid,
                             std::uint32_t& age) {
    guid.clear();
    age = 0;
    if (nt->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_DEBUG) {
        return;
    }
    const IMAGE_DATA_DIRECTORY& data =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    if (data.VirtualAddress == 0 ||
        data.Size < sizeof(IMAGE_DEBUG_DIRECTORY) ||
        !AddFits(data.VirtualAddress, data.Size, nt->OptionalHeader.SizeOfImage)) {
        return;
    }
    const IMAGE_DEBUG_DIRECTORY* directories =
        reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(base + data.VirtualAddress);
    const std::size_t count = data.Size / sizeof(IMAGE_DEBUG_DIRECTORY);
    for (std::size_t i = 0; i < count; ++i) {
        if (directories[i].Type != IMAGE_DEBUG_TYPE_CODEVIEW ||
            directories[i].AddressOfRawData == 0 ||
            directories[i].SizeOfData < sizeof(DWORD) + sizeof(GUID) + sizeof(DWORD) ||
            !AddFits(directories[i].AddressOfRawData, directories[i].SizeOfData,
                     nt->OptionalHeader.SizeOfImage)) {
            continue;
        }
        const std::uint8_t* record = base + directories[i].AddressOfRawData;
        DWORD signature = 0;
        memcpy(&signature, record, sizeof(signature));
        if (signature != 0x53445352u) { // "RSDS" in little-endian order.
            continue;
        }
        GUID value = {};
        memcpy(&value, record + sizeof(DWORD), sizeof(value));
        memcpy(&age, record + sizeof(DWORD) + sizeof(GUID), sizeof(age));
        guid = GuidString(value);
        return;
    }
}

bool HashSha256(const std::uint8_t* data,
                std::size_t size,
                std::string& result,
                std::string& error) {
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    if (!CryptAcquireContextA(&provider, NULL, NULL, PROV_RSA_AES,
                              CRYPT_VERIFYCONTEXT)) {
        error = "CryptAcquireContext(PROV_RSA_AES) failed";
        return false;
    }
    if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
        CryptReleaseContext(provider, 0);
        error = "CryptCreateHash(CALG_SHA_256) failed";
        return false;
    }

    bool ok = true;
    std::size_t consumed = 0;
    while (consumed < size) {
        const DWORD amount = static_cast<DWORD>(std::min<std::size_t>(
            size - consumed, static_cast<std::size_t>(0x40000000u)));
        if (!CryptHashData(hash, data + consumed, amount, 0)) {
            ok = false;
            error = "CryptHashData failed";
            break;
        }
        consumed += amount;
    }

    BYTE digest[32] = {};
    DWORD digestSize = sizeof(digest);
    if (ok && !CryptGetHashParam(hash, HP_HASHVAL, digest, &digestSize, 0)) {
        ok = false;
        error = "CryptGetHashParam failed";
    }
    if (ok) {
        std::ostringstream stream;
        stream << std::hex << std::setfill('0');
        for (DWORD i = 0; i < digestSize; ++i) {
            stream << std::setw(2) << static_cast<unsigned int>(digest[i]);
        }
        result = stream.str();
    }

    CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);
    return ok;
}

bool HashRawTextSection(std::string& hash, std::string& error) {
    char path[32768] = {};
    const DWORD pathLength = GetModuleFileNameA(NULL, path, sizeof(path));
    if (pathLength == 0 || pathLength >= sizeof(path)) {
        error = "GetModuleFileNameA failed for the main executable";
        return false;
    }

    ScopedHandle file(CreateFileA(path, GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
    if (file.get() == INVALID_HANDLE_VALUE) {
        error = "could not open the running executable for fingerprinting";
        return false;
    }

    LARGE_INTEGER fileSize = {};
    if (!GetFileSizeEx(file.get(), &fileSize) || fileSize.QuadPart <= 0) {
        error = "could not query executable size";
        return false;
    }
    ScopedHandle mapping(CreateFileMappingA(file.get(), NULL, PAGE_READONLY, 0, 0, NULL));
    if (mapping.get() == NULL) {
        error = "CreateFileMapping failed for the executable";
        return false;
    }
    ScopedMap view(MapViewOfFile(mapping.get(), FILE_MAP_READ, 0, 0, 0));
    if (view.bytes() == NULL) {
        error = "MapViewOfFile failed for the executable";
        return false;
    }

    const std::uint64_t total = static_cast<std::uint64_t>(fileSize.QuadPart);
    if (!AddFits(0, sizeof(IMAGE_DOS_HEADER), total)) {
        error = "executable is too small for a DOS header";
        return false;
    }
    const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(view.bytes());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 ||
        !AddFits(static_cast<std::uint32_t>(dos->e_lfanew),
                 sizeof(IMAGE_NT_HEADERS32), total)) {
        error = "invalid PE header while hashing .text";
        return false;
    }
    const IMAGE_NT_HEADERS32* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        view.bytes() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        error = "the running executable on disk is not PE32";
        return false;
    }

    const std::uint64_t sectionTable = static_cast<std::uint64_t>(dos->e_lfanew) +
        sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + nt->FileHeader.SizeOfOptionalHeader;
    const std::uint64_t sectionBytes = static_cast<std::uint64_t>(
        nt->FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
    if (!AddFits(sectionTable, sectionBytes, total)) {
        error = "PE section table lies outside the executable";
        return false;
    }

    const IMAGE_SECTION_HEADER* sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(
        view.bytes() + sectionTable);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        char name[9] = {};
        memcpy(name, sections[i].Name, 8);
        if (strcmp(name, ".text") != 0) {
            continue;
        }
        const std::uint64_t offset = sections[i].PointerToRawData;
        const std::uint64_t size = sections[i].SizeOfRawData;
        if (!AddFits(offset, size, total)) {
            error = "raw .text section lies outside the executable";
            return false;
        }
        return HashSha256(view.bytes() + offset, static_cast<std::size_t>(size),
                          hash, error);
    }

    error = "the running executable has no .text section";
    return false;
}

} // namespace

std::uintptr_t GameBase() {
    return reinterpret_cast<std::uintptr_t>(GetModuleHandleW(NULL));
}

std::uint32_t GameImageSize() {
    const std::uintptr_t base = GameBase();
    if (base == 0) {
        return 0;
    }
    __try {
        const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0) {
            return 0;
        }
        const IMAGE_NT_HEADERS32* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
            base + static_cast<std::uint32_t>(dos->e_lfanew));
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            return 0;
        }
        return nt->OptionalHeader.SizeOfImage;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

bool FingerprintRunningGame(GameBuild& result, std::string& error) {
    const std::uintptr_t base = GameBase();
    if (base == 0) {
        error = "GetModuleHandle(NULL) returned null";
        return false;
    }

    __try {
        const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0) {
            error = "main module has an invalid DOS header";
            return false;
        }
        const IMAGE_NT_HEADERS32* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
            base + static_cast<std::uint32_t>(dos->e_lfanew));
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            error = "main module is not a valid PE32 image";
            return false;
        }
        result.machine = nt->FileHeader.Machine;
        result.timeDateStamp = nt->FileHeader.TimeDateStamp;
        result.imageSize = nt->OptionalHeader.SizeOfImage;
        result.entryPointRva = nt->OptionalHeader.AddressOfEntryPoint;
        ReadCodeViewFingerprint(reinterpret_cast<const std::uint8_t*>(base), nt,
                                result.pdbGuid, result.pdbAge);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        error = "access violation while reading the main module PE headers";
        return false;
    }

    return HashRawTextSection(result.textSha256, error);
}

bool MatchesSupportedBuild(const GameBuild& build, std::string& mismatch) {
    if (build.machine != kScreamPublicDev.machine) {
        mismatch = "PE machine does not match the analyzed x86 build";
    } else if (build.timeDateStamp != kScreamPublicDev.timeDateStamp) {
        mismatch = "COFF timestamp does not match the analyzed build";
    } else if (build.imageSize != kScreamPublicDev.imageSize) {
        mismatch = "SizeOfImage does not match the analyzed build";
    } else if (build.entryPointRva != kScreamPublicDev.entryPointRva) {
        mismatch = "entry-point RVA does not match the analyzed build";
    } else if (build.pdbGuid != kScreamPublicDev.pdbGuid ||
               build.pdbAge != kScreamPublicDev.pdbAge) {
        mismatch = "PDB CodeView GUID/age does not match the analyzed build";
    } else if (build.textSha256 != kScreamPublicDev.textSha256) {
        mismatch = "raw .text SHA-256 does not match the analyzed build";
    } else {
        mismatch.clear();
        return true;
    }
    return false;
}

} // namespace Build
} // namespace Scream
