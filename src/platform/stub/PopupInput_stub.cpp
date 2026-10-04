#include "platform/PopupInput.h"
PopupInput::PopupInput(QObject* parent) : QObject(parent) {}
PopupInput::~PopupInput() = default;
void PopupInput::start(QWidget*, platform::PasteTarget) {}
void PopupInput::stop() {}
bool PopupInput::nativeEventFilter(const QByteArray&, void*, qintptr*) { return false; }
