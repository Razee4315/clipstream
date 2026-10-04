#pragma once

#include <QtGlobal>
#include <functional>
class QObject;

namespace platform {

struct PasteTarget {
    quintptr window = 0;
    quint32 process = 0;
    quintptr focus = 0;
};

PasteTarget capturePasteTarget();
// False when another app held the clipboard open and our write was dropped.
bool ownsClipboard();
bool restorePasteTarget(PasteTarget target);
void pasteToTarget(PasteTarget target, QObject* context, std::function<void(bool)> finished);
void setPopupNonActivating(quintptr window, bool enabled);
bool handlePopupNativeEvent(void* message, qintptr* result);

} // namespace platform
