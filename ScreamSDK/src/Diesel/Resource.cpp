#include "Diesel/Resource.hpp"

#include "Diesel/Core.hpp"

namespace Diesel {
namespace Resource {

VirtualFileSystem* GetVirtualFileSystem() {
    return Core::GetVirtualFileSystem();
}

MainDatabase* GetMainDatabase() {
    return Core::GetMainDatabase();
}

} // namespace Resource
} // namespace Diesel

