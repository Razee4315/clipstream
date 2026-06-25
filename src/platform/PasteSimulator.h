#pragma once

namespace platform {

// Simulate the OS paste shortcut (Ctrl+V on Windows/Linux, Cmd+V on macOS) so
// the front-most app receives whatever we just put on the clipboard.
void simulatePaste();

} // namespace platform
