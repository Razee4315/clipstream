#include "platform/PasteSimulator.h"
namespace platform {
PasteTarget capturePasteTarget() { return {}; }
bool ownsClipboard() { return true; }
QRect caretRect(PasteTarget) { return {}; }
void prepareCaretLookup() {}
bool restorePasteTarget(PasteTarget) { return false; }
void pasteToTarget(PasteTarget, QObject*, std::function<void(bool)> finished) { finished(false); }
void setPopupNonActivating(quintptr, bool) {}
bool handlePopupNativeEvent(void*, qintptr*) { return false; }
}
