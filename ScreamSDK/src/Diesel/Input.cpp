#include "Diesel/Input.hpp"

#include "Diesel/Core.hpp"

namespace Diesel {
namespace InputSystem {

Input* Get() {
    return Core::GetInput();
}

} // namespace InputSystem
} // namespace Diesel

