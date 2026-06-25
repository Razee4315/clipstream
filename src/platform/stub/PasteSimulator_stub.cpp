#include "platform/PasteSimulator.h"

namespace platform {

// Non-Windows stub. Auto-paste will use XTest (X11) and CGEvent (macOS, requires
// Accessibility permission) in a later phase.
void simulatePaste() {}

} // namespace platform
