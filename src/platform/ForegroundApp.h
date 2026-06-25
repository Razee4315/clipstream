#pragma once

#include <QString>

namespace platform {

// Best-effort name of the application that owns the foreground window at the
// moment of the call (e.g. "chrome.exe"). Empty when unknown/unsupported.
// Used to tag clipboard entries with their source app.
QString foregroundAppName();

} // namespace platform
