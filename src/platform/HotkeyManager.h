#pragma once

#include <QObject>
#include <Qt>
#include <memory>

// Cross-platform global hotkey registration. The header is the contract; the
// implementation is selected at build time (Windows: RegisterHotKey; others:
// stub until X11/Cocoa land). Emits activated() when a registered combo fires.
class HotkeyManager : public QObject {
    Q_OBJECT
public:
    explicit HotkeyManager(QObject* parent = nullptr);
    ~HotkeyManager() override;

    // Register a system-wide hotkey identified by `id`. Returns false if the OS
    // refused it (already taken, unsupported, or no implementation on this OS).
    bool registerHotkey(int id, Qt::KeyboardModifiers mods, Qt::Key key);
    void unregisterHotkey(int id);

signals:
    void activated(int id);

private:
    class Impl;                 // platform-private, defined in the chosen .cpp
    std::unique_ptr<Impl> d;
};
