#pragma once

#include <QRect>
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
// Where the destination's text caret is, in native screen pixels. Empty when the
// app does not report one (the popup then opens beside the mouse pointer).
QRect caretRect(PasteTarget target);
// Start the caret lookup machinery ahead of the first popup.
void prepareCaretLookup();
bool restorePasteTarget(PasteTarget target);
void pasteToTarget(PasteTarget target, QObject* context, std::function<void(bool)> finished);
void setPopupNonActivating(quintptr window, bool enabled);
bool handlePopupNativeEvent(void* message, qintptr* result);

} // namespace platform
