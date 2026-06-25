#include "platform/Autostart.h"

namespace platform {

// Non-Windows stub. Linux (.desktop autostart) and macOS (LaunchAgent) versions
// arrive with their platform layers.
bool isLaunchAtStartupEnabled() { return false; }
void setLaunchAtStartup(bool) {}

} // namespace platform
