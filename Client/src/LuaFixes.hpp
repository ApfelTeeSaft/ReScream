#pragma once

namespace Client {
namespace LuaFixes {

bool Install();
void Quiesce();
bool Remove();
bool IsF4FixApplied();
bool IsKeybindApplied();
bool IsFovFeatureApplied();
bool IsPerformanceFeatureApplied();
bool IsQualityOfLifeFeatureApplied();
bool IsLobbyBrowserFixApplied();
bool IsVersionOverrideApplied();
bool IsStaticBundleProbeSeen();

} // namespace LuaFixes
} // namespace Client
