#pragma once

namespace platform {

// Launch-at-login integration. On Windows this is the HKCU "Run" registry key;
// stubbed elsewhere until the Linux .desktop / macOS LaunchAgent versions land.
bool isLaunchAtStartupEnabled();
void setLaunchAtStartup(bool enabled);

} // namespace platform
