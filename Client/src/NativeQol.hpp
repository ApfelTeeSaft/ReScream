#pragma once

#include <cstddef>

namespace Client {
namespace NativeQol {

bool Install();
bool Remove();
void Tick(bool borderlessRequested, bool rendererTransitionPending);
void FormatNetworkDebugText(char* destination, std::size_t capacity);
bool IsInstalled();
bool IsBorderlessApplied();

} // namespace NativeQol
} // namespace Client
