#include "platform/ForegroundApp.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace platform {

QString foregroundAppName() {
    HWND hwnd = ::GetForegroundWindow();
    if (!hwnd)
        return {};

    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0)
        return {};

    HANDLE proc = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc)
        return {};

    QString name;
    wchar_t buffer[MAX_PATH];
    DWORD size = MAX_PATH;
    if (::QueryFullProcessImageNameW(proc, 0, buffer, &size)) {
        const QString path = QString::fromWCharArray(buffer, static_cast<int>(size));
        name = path.section('\\', -1); // keep just the exe file name
    }
    ::CloseHandle(proc);
    return name;
}

} // namespace platform
