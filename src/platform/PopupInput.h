#pragma once

#include "platform/PasteSimulator.h"
#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QVector>

class QTimer;
class QWidget;

// Temporary navigation shortcuts while the popup leaves the destination focused.
// No text is intercepted. Removed when searching, opening a dialog, or closing.
class PopupInput : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit PopupInput(QObject* parent = nullptr);
    ~PopupInput() override;
    void start(QWidget* popup, platform::PasteTarget target);
    void stop();
    bool nativeEventFilter(const QByteArray&, void*, qintptr*) override;
signals:
    void shortcut(int key, Qt::KeyboardModifiers modifiers);
    void dismissRequested();
private:
    struct Binding { int id; int key; Qt::KeyboardModifiers modifiers; };
    QVector<Binding> m_bindings;
    QTimer* m_timer = nullptr;
    bool m_mouseDown = false;
};
