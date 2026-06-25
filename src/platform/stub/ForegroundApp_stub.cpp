#include "platform/ForegroundApp.h"

namespace platform {

// Non-Windows stub. Source-app detection will use NSWorkspace (macOS) and
// X11/Wayland queries (Linux) in a later phase.
QString foregroundAppName() { return {}; }

} // namespace platform
