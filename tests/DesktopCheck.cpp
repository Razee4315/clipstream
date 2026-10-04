// Optional interactive Windows smoke test. Uses temporary history and a separate
// editor process, so focus/selection crosses a real process boundary.
//
// It takes the foreground and sends real key and mouse input, so it waits for the
// keyboard and mouse to be idle and only ever injects input while its own scratch
// editor is the foreground window.
#include "core/ClipboardMonitor.h"
#include "core/Database.h"
#include "platform/HotkeyManager.h"
#include "ui/OverlayWindow.h"
#include "theme.h"
#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QScreen>
#include <QTextStream>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QVBoxLayout>
#include <QClipboard>
#include <QMimeData>
#include <QListView>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <memory>
#include <utility>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {
const wchar_t* const kEditorClass = L"ClipStreamScratch";
// The process id keeps a slow-to-exit editor from an earlier run from being found.
QString editorTitle(qint64 pid) {
    return QStringLiteral("ClipStream native paste target %1").arg(pid);
}
}

#define PRESS(vk) QVERIFY2(key(vk, true), "Another window took focus; no input was sent."); key(vk, false)

class WindowsPasteTests : public QObject {
    Q_OBJECT
    QProcess editor;
    HWND target = nullptr;
    HWND edit = nullptr;
    HWND previous = nullptr;
    bool clipboardSaved = false;
    QList<std::pair<QString, QByteArray>> savedClipboard;
    QWidget bootstrap;
    QTemporaryDir directory;
    std::unique_ptr<Database> db;
    std::unique_ptr<OverlayWindow> overlay;
    QString text() const {
        wchar_t buffer[512] = {};
        ::SendMessageW(edit, WM_GETTEXT, 512, reinterpret_cast<LPARAM>(buffer));
        return QString::fromWCharArray(buffer);
    }
    // Real input lands in whichever window is in front, so a key is only pressed
    // while the scratch editor is. Releases are always sent so nothing stays held.
    bool key(WORD vk, bool down) {
        if (down && ::GetForegroundWindow() != target) return false;
        INPUT input{}; input.type = INPUT_KEYBOARD; input.ki.wVk = vk;
        if (!down) input.ki.dwFlags = KEYEVENTF_KEYUP;
        ::SendInput(1, &input, sizeof(INPUT));
        QTest::qWait(25);
        return true;
    }
    bool openWithHotkey() {
        const bool sent = key(VK_CONTROL, true) && key(VK_SHIFT, true) && key(VK_F11, true);
        key(VK_F11, false); key(VK_SHIFT, false); key(VK_CONTROL, false);
        return sent;
    }
    void clickFirst() {
        auto* list = overlay->findChild<QListView*>();
        const auto rect = list->visualRect(list->model()->index(0, 0));
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, rect.topLeft() + QPoint(80, 14));
    }
    void restoreClipboard() {
        if (!clipboardSaved) return;
        // A busy clipboard drops the write, so confirm it and try again.
        for (int attempt = 0; attempt < 10; ++attempt) {
            auto* data = new QMimeData;
            for (const auto& format : savedClipboard) data->setData(format.first, format.second);
            QApplication::clipboard()->setMimeData(data);
            if (platform::ownsClipboard()) return;
            QTest::qWait(50);
        }
    }
private slots:
    void initTestCase() {
        const auto idle = [] {
            LASTINPUTINFO info{}; info.cbSize = sizeof(LASTINPUTINFO);
            ::GetLastInputInfo(&info);
            return ::GetTickCount() - info.dwTime;
        };
        QElapsedTimer waiting; waiting.start();
        while (idle() < 3000) {
            if (waiting.elapsed() > 60000)
                QSKIP("The desktop is in use. Run again when the keyboard and mouse are idle.");
            QTest::qWait(200);
        }
        previous = ::GetForegroundWindow();
        const auto* current = QApplication::clipboard()->mimeData();
        if (current) for (const QString& format : current->formats()) savedClipboard.append({format, current->data(format)});
        clipboardSaved = true;
        bootstrap.setWindowTitle(QStringLiteral("ClipStream automated desktop check"));
        bootstrap.resize(300, 80); bootstrap.show(); bootstrap.activateWindow();
        editor.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--native-editor")});
        QVERIFY(editor.waitForStarted());
        const QString title = editorTitle(editor.processId());
        QTRY_VERIFY_WITH_TIMEOUT((target = ::FindWindowW(kEditorClass, reinterpret_cast<const wchar_t*>(title.utf16()))) != nullptr, 5000);
        edit = ::GetDlgItem(target, 42);
        QVERIFY(edit);
        db = std::make_unique<Database>();
        QVERIFY(db->open(directory.path()));
        ClipEntry entry; entry.content = QStringLiteral("ClipStream pasted correctly");
        db->insertEntry(entry);
        overlay = std::make_unique<OverlayWindow>(db.get(), nullptr);
    }
    void init() {
        // Other clipboard listeners read for a few hundred milliseconds after every
        // change; start each test from a quiet clipboard like a person would.
        QTest::qWait(400);
        QVERIFY(platform::restorePasteTarget({reinterpret_cast<quintptr>(target), static_cast<quint32>(editor.processId())}));
        QTRY_COMPARE(::GetForegroundWindow(), target);
        ::SendMessageW(edit, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(L"Before REPLACE ME after"));
        ::SendMessageW(edit, EM_SETSEL, 7, 17);
    }
    void cleanup() { if (overlay) overlay->hide(); }
    void opensWithoutLosingFocusAndClickReplacesSelection() {
        overlay->showAtCursor();
        QTest::qWait(160);
        QCOMPARE(::GetForegroundWindow(), target);
        const auto selection = ::SendMessageW(edit, EM_GETSEL, 0, 0);
        QCOMPARE(LOWORD(selection), WORD(7));
        QCOMPARE(HIWORD(selection), WORD(17));
        QCOMPARE(::SendMessageW(reinterpret_cast<HWND>(overlay->winId()), WM_MOUSEACTIVATE,
            reinterpret_cast<WPARAM>(target), MAKELPARAM(HTCLIENT, WM_LBUTTONDOWN)), LRESULT(MA_NOACTIVATE));
        clickFirst();
        QTRY_COMPARE(text(), QStringLiteral("Before ClipStream pasted correctly after"));
        QCOMPARE(::GetForegroundWindow(), target);
    }
    // Like the Windows clipboard panel: the popup opens under the text caret.
    void opensAtTextCaret() {
        const QRect caret = platform::caretRect(platform::capturePasteTarget()); // native pixels
        QVERIFY(!caret.isEmpty());
        const QPoint saved = QCursor::pos();
        auto restoreCursor = qScopeGuard([saved] { QCursor::setPos(saved); });
        // Park the pointer far away so the caret, not the mouse, must be the anchor.
        QCursor::setPos(QGuiApplication::primaryScreen()->availableGeometry().bottomRight());
        overlay->showAtCursor();
        QTest::qWait(130);
        const qreal scale = overlay->devicePixelRatioF();
        // The visible card sits inside the transparent shadow gutter.
        const QPointF card = QPointF(overlay->pos() + QPoint(Theme::ShadowMargin, Theme::ShadowMargin)) * scale;
        QVERIFY2(qAbs(card.x() - caret.left()) <= 3 * scale,
                 qPrintable(QStringLiteral("card x %1, caret x %2").arg(card.x()).arg(caret.left())));
        QVERIFY2(qAbs(card.y() - (caret.bottom() + 6 * scale)) <= 3 * scale,
                 qPrintable(QStringLiteral("card y %1, caret bottom %2").arg(card.y()).arg(caret.bottom())));
        db->setSetting(QStringLiteral("popup_anchor"), QStringLiteral("mouse"));
        overlay->hide();
        QCursor::setPos(QGuiApplication::primaryScreen()->availableGeometry().center());
        overlay->showAtCursor();
        QTest::qWait(130);
        db->setSetting(QStringLiteral("popup_anchor"), QStringLiteral("caret"));
        const QPoint pointer = QCursor::pos();
        QVERIFY(qAbs(overlay->x() + Theme::ShadowMargin - pointer.x()) <= 3);
    }
    void searchRestoresOriginalSelection() {
        overlay->showAtCursor();
        QTest::qWait(130);
        auto* search = overlay->findChild<QLineEdit*>(QStringLiteral("search"));
        QTest::keyClick(search, Qt::Key_F, Qt::ControlModifier);
        QTest::keyClicks(search, "pasted");
        QTRY_COMPARE(::GetForegroundWindow(), reinterpret_cast<HWND>(overlay->winId()));
        clickFirst();
        QTRY_COMPARE(text(), QStringLiteral("Before ClipStream pasted correctly after"));
        QCOMPARE(::GetForegroundWindow(), target);
    }
    void pasteWaitsForModifierRelease() {
        const auto destination = platform::capturePasteTarget();
        QApplication::clipboard()->setText(QStringLiteral("replacement"));
        QVERIFY(platform::ownsClipboard());
        QVERIFY2(key(VK_SHIFT, true), "Another window took focus; no input was sent.");
        auto release = qScopeGuard([this] { key(VK_SHIFT, false); });
        bool done = false, ok = false;
        platform::pasteToTarget(destination, this, [&](bool result) { done = true; ok = result; });
        QTest::qWait(100);
        const bool waited = !done && text() == QStringLiteral("Before REPLACE ME after");
        release.dismiss(); key(VK_SHIFT, false);
        QVERIFY(waited);
        QTRY_VERIFY(done); QVERIFY(ok);
        QTRY_COMPARE(text(), QStringLiteral("Before replacement after"));
    }
    // Another program holding the clipboard open makes the write fail silently.
    // The previous clipboard content must never be pasted in its place.
    void busyClipboardNeverPastesStaleContent() {
        QApplication::clipboard()->setText(QStringLiteral("stale content"));
        QTest::qWait(400);
        QSignalSpy busy(overlay.get(), &OverlayWindow::clipboardBusy);
        QSignalSpy failed(overlay.get(), &OverlayWindow::pasteFailed);
        overlay->showAtCursor();
        QTest::qWait(130);
        const HWND holder = reinterpret_cast<HWND>(bootstrap.winId());
        bool held = false;
        for (int attempt = 0; attempt < 40 && !(held = ::OpenClipboard(holder)); ++attempt) QTest::qWait(25);
        QVERIFY(held);
        clickFirst();
        ::CloseClipboard();
        QCOMPARE(busy.size(), 1);
        QVERIFY(overlay->isVisible()); // still open, so the clip can be picked again
        QTest::qWait(250);
        QCOMPARE(text(), QStringLiteral("Before REPLACE ME after"));
        clickFirst();
        QTRY_COMPARE(text(), QStringLiteral("Before ClipStream pasted correctly after"));
        QCOMPARE(busy.size(), 1);
        QCOMPARE(failed.size(), 0);
    }
    // The real path: a registered global hotkey opens the popup, then keys the
    // user types while the destination keeps focus drive the popup.
    void globalHotkeyAndKeyboardPasteWithoutFocus() {
        HotkeyManager hotkey;
        // Not Ctrl+Shift+V: an installed ClipStream may already own that one.
        QVERIFY(hotkey.registerHotkey(77, Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_F11));
        connect(&hotkey, &HotkeyManager::activated, overlay.get(), [this] { overlay->toggleAtCursor(); });
        QVERIFY2(openWithHotkey(), "Another window took focus; no input was sent.");
        QTRY_VERIFY(overlay->isVisible());
        QTest::qWait(120);
        QCOMPARE(::GetForegroundWindow(), target);
        PRESS(VK_ESCAPE);
        QTRY_VERIFY(!overlay->isVisible());
        QCOMPARE(::GetForegroundWindow(), target);
        QCOMPARE(text(), QStringLiteral("Before REPLACE ME after"));

        QVERIFY2(openWithHotkey(), "Another window took focus; no input was sent.");
        QTRY_VERIFY(overlay->isVisible());
        QTest::qWait(120);
        PRESS(VK_DOWN);
        PRESS(VK_UP);
        PRESS(VK_RETURN);
        QTRY_COMPARE(text(), QStringLiteral("Before ClipStream pasted correctly after"));
        QVERIFY(!overlay->isVisible());
        QCOMPARE(::GetForegroundWindow(), target);
        // Navigation keys reach the destination again once the popup is gone.
        ::SendMessageW(edit, EM_SETSEL, 0, 0);
        PRESS(VK_RIGHT);
        QTRY_COMPARE(LOWORD(::SendMessageW(edit, EM_GETSEL, 0, 0)), WORD(1));
    }
    void quickPasteWaitsForCtrlRelease() {
        overlay->showAtCursor();
        QTest::qWait(130);
        QVERIFY2(key(VK_CONTROL, true), "Another window took focus; no input was sent.");
        auto release = qScopeGuard([this] { key('1', false); key(VK_CONTROL, false); });
        QVERIFY2(key('1', true), "Another window took focus; no input was sent.");
        key('1', false);
        QTRY_VERIFY(!overlay->isVisible());
        QTest::qWait(120);
        const bool waited = text() == QStringLiteral("Before REPLACE ME after");
        release.dismiss(); key(VK_CONTROL, false);
        QVERIFY(waited);
        QTRY_COMPARE(text(), QStringLiteral("Before ClipStream pasted correctly after"));
    }
    void outsideClickDismissesWithoutPasting() {
        const QPoint saved = QCursor::pos();
        auto restoreCursor = qScopeGuard([saved] { QCursor::setPos(saved); });
        // Open the popup in the far corner so it cannot cover the destination.
        QCursor::setPos(QGuiApplication::primaryScreen()->availableGeometry().bottomRight());
        overlay->showAtCursor();
        QTest::qWait(130);
        RECT frame{}; ::GetWindowRect(target, &frame);
        const QPoint titleBar = QPoint(frame.left + 200, frame.top + 12) / overlay->devicePixelRatioF();
        if (overlay->geometry().contains(titleBar))
            QSKIP("Screen too small to click beside the popup.");
        QCursor::setPos(titleBar);
        QTest::qWait(60);
        POINT under{}; ::GetCursorPos(&under);
        QVERIFY2(::GetForegroundWindow() == target && ::GetAncestor(::WindowFromPoint(under), GA_ROOT) == target,
                 "Another window is in the way; no click was sent.");
        INPUT click[2] = {};
        click[0].type = click[1].type = INPUT_MOUSE;
        click[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN; click[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        ::SendInput(1, &click[0], sizeof(INPUT));
        QTest::qWait(60);
        ::SendInput(1, &click[1], sizeof(INPUT));
        QTRY_VERIFY(!overlay->isVisible());
        QCOMPARE(::GetForegroundWindow(), target);
        QCOMPARE(text(), QStringLiteral("Before REPLACE ME after"));
    }
    // Password managers mark secrets with a registered Windows clipboard format.
    void clipboardPrivacyFormatIsHonoured() {
        ClipboardMonitor monitor;
        QSignalSpy captured(&monitor, &ClipboardMonitor::textCaptured);
        const HWND owner = reinterpret_cast<HWND>(bootstrap.winId());
        const auto copy = [owner](const wchar_t* value, bool markPrivate) {
            for (int attempt = 0; !::OpenClipboard(owner); ++attempt) {
                if (attempt == 40) return false;
                QTest::qWait(25);
            }
            ::EmptyClipboard();
            const size_t bytes = (::lstrlenW(value) + 1) * sizeof(wchar_t);
            HGLOBAL data = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
            memcpy(::GlobalLock(data), value, bytes); ::GlobalUnlock(data);
            ::SetClipboardData(CF_UNICODETEXT, data);
            if (markPrivate) {
                HGLOBAL flag = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DWORD));
                ::SetClipboardData(::RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing"), flag);
            }
            ::CloseClipboard();
            return true;
        };
        QVERIFY(copy(L"desktop-check secret", true));
        QTest::qWait(500);
        QCOMPARE(captured.size(), 0);
        QVERIFY(copy(L"desktop-check ordinary", false));
        QTRY_COMPARE(captured.size(), 1);
        QCOMPARE(captured.first().first().toString(), QStringLiteral("desktop-check ordinary"));
    }
    void invalidTargetDoesNotPaste() {
        bool done = false, ok = true;
        platform::pasteToTarget({}, this, [&](bool result) { done = true; ok = result; });
        QVERIFY(done); QVERIFY(!ok);
        QCOMPARE(text(), QStringLiteral("Before REPLACE ME after"));
    }
    void cleanupTestCase() {
        overlay.reset(); db.reset();
        if (target) ::PostMessageW(target, WM_CLOSE, 0, 0);
        if (editor.state() != QProcess::NotRunning && !editor.waitForFinished(2000)) { editor.kill(); editor.waitForFinished(); }
        restoreClipboard();
        bootstrap.hide();
        if (previous && ::IsWindow(previous)) ::SetForegroundWindow(previous);
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ClipStreamDesktopCheck"));
    QFont font(Theme::fontFamily()); font.setPixelSize(13); app.setFont(font);
    Theme::setThemeId(QStringLiteral("dark"));
    if (app.arguments().contains(QStringLiteral("--native-editor"))) {
        WNDCLASSW cls{};
        cls.lpszClassName = kEditorClass;
        cls.hInstance = ::GetModuleHandleW(nullptr);
        cls.lpfnWndProc = [](HWND window, UINT message, WPARAM wparam, LPARAM lparam) -> LRESULT {
            if (message == WM_SETFOCUS) { ::SetFocus(::GetDlgItem(window, 42)); return 0; }
            return ::DefWindowProcW(window, message, wparam, lparam);
        };
        ::RegisterClassW(&cls);
        const QString title = editorTitle(QCoreApplication::applicationPid());
        HWND window = ::CreateWindowExW(0, kEditorClass, reinterpret_cast<const wchar_t*>(title.utf16()),
            WS_OVERLAPPEDWINDOW, 100, 100, 600, 240, nullptr, nullptr, nullptr, nullptr);
        HWND edit = ::CreateWindowExW(0, L"EDIT", L"Before REPLACE ME after",
            WS_VISIBLE | WS_CHILD | ES_MULTILINE | WS_BORDER, 10, 10, 560, 170,
            window, reinterpret_cast<HMENU>(42), nullptr, nullptr);
        ::ShowWindow(window, SW_SHOW); ::SetForegroundWindow(window); ::SetFocus(edit);
        ::SendMessageW(edit, EM_SETSEL, 7, 17);
        QTimer timer; timer.setInterval(100);
        QObject::connect(&timer, &QTimer::timeout, &app, [&] { if (!::IsWindow(window)) app.quit(); });
        timer.start();
        return app.exec();
    }
    // Timing harness: shows the popup without taking focus or sending input and
    // measures the interactions that have to feel instant. Optional argument: a
    // data folder to measure instead of the synthetic history.
    if (app.arguments().contains(QStringLiteral("--bench"))) {
        const int at = app.arguments().indexOf(QStringLiteral("--bench"));
        const QString folder = app.arguments().value(at + 1);
        QTemporaryDir scratch;
        Database db;
        if (!db.open(folder.isEmpty() ? scratch.path() : folder)) return 1;
        if (folder.isEmpty()) {
            QImage picture(1920, 1080, QImage::Format_RGB32);
            for (int i = 0; i < 300; ++i) {
                ClipEntry entry;
                entry.sourceApp = QStringLiteral("Bench.exe");
                if (i % 25 == 0) {
                    picture.fill(QColor::fromHsv(i % 360, 160, 200));
                    entry.type = ContentType::Image;
                    entry.imagePath = db.imagesDir() + QStringLiteral("/bench_%1.png").arg(i);
                    picture.save(entry.imagePath);
                    entry.content = QStringLiteral("Image 1920×1080");
                } else if (i % 40 == 7) {
                    entry.content = QStringLiteral("line of a large log file %1\n").arg(i).repeated(40000); // ~1 MB
                } else if (i % 5 == 0) {
                    entry.type = ContentType::Url;
                    entry.content = QStringLiteral("https://example.com/page/%1").arg(i);
                } else {
                    entry.content = QStringLiteral("Clip number %1 with a normal sentence of text.").arg(i);
                }
                db.insertEntry(entry);
            }
        }
        QFile report(QCoreApplication::applicationDirPath() + QStringLiteral("/artifacts/bench.txt"));
        QDir().mkpath(QCoreApplication::applicationDirPath() + QStringLiteral("/artifacts"));
        if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) return 1;
        QTextStream out(&report);
        QElapsedTimer clock;
        const auto settle = [] { for (int i = 0; i < 5; ++i) { QCoreApplication::processEvents(); QTest::qWait(10); } };
        const auto timed = [&](const QString& label, const std::function<void()>& action) {
            clock.start();
            action();
            QCoreApplication::processEvents(); // deliver the resulting layout and paint
            out << QStringLiteral("%1 ms  %2\n").arg(clock.nsecsElapsed() / 1e6, 8, 'f', 1).arg(label);
            out.flush();
            settle();
        };
        OverlayWindow overlay(&db, nullptr);
        platform::setPopupNonActivating(overlay.winId(), true);
        const QRect area = QGuiApplication::primaryScreen()->availableGeometry();
        overlay.move(area.right() - overlay.width(), area.bottom() - overlay.height());
        timed(QStringLiteral("warm-up at startup"), [&] { overlay.warmUp(); });
        timed(QStringLiteral("first show"), [&] { overlay.show(); });
        QTest::qWait(300);
        timed(QStringLiteral("full repaint"), [&] { overlay.repaint(); });
        auto* list = overlay.findChild<QListView*>();
        const auto filters = overlay.findChildren<QPushButton*>(QStringLiteral("filter"));
        for (int round = 0; round < 2; ++round)
            for (int i : {3, 2, 4, 1, 0})
                timed(QStringLiteral("click filter '%1' (round %2)").arg(filters[i]->text()).arg(round + 1),
                      [&] { QTest::mouseClick(filters[i], Qt::LeftButton); });
        for (int row = 0; row < 6 && row < list->model()->rowCount(); ++row)
            timed(QStringLiteral("hover row %1").arg(row), [&] {
                QTest::mouseMove(list->viewport(), list->visualRect(list->model()->index(row, 0)).center());
            });
        for (int row = 1; row <= 6 && row < list->model()->rowCount(); ++row)
            timed(QStringLiteral("select row %1").arg(row), [&] { list->setCurrentIndex(list->model()->index(row, 0)); });
        timed(QStringLiteral("scroll to bottom"), [&] { list->scrollToBottom(); });
        timed(QStringLiteral("scroll to top"), [&] { list->scrollToTop(); });
        auto* search = overlay.findChild<QLineEdit*>(QStringLiteral("search"));
        for (const char* query : {"c", "cl", "cli", "clip", ""})
            timed(QStringLiteral("search '%1'").arg(QLatin1String(query)), [&] { search->setText(QLatin1String(query)); });
        // Where a reload spends its time: the query alone, then the whole refresh.
        const auto average = [&](const QString& label, const std::function<void()>& action) {
            clock.start();
            for (int i = 0; i < 20; ++i) action();
            out << QStringLiteral("%1 ms  %2 (average of 20)\n").arg(clock.nsecsElapsed() / 20e6, 8, 'f', 2).arg(label);
        };
        average(QStringLiteral("query: list previews"), [&] { db.search(QString(), 200, ClipFilter::All, true); });
        average(QStringLiteral("query: search 'clip'"), [&] { db.search(QStringLiteral("clip"), 200, ClipFilter::All, true); });
        average(QStringLiteral("reload without painting"), [&] { overlay.reload(); });
        average(QStringLiteral("reload and paint"), [&] { overlay.reload(); QCoreApplication::processEvents(); });
        average(QStringLiteral("repaint whole popup"), [&] { overlay.repaint(); });
        // What the compositor actually shows, at this display's scaling.
        QGuiApplication::primaryScreen()->grabWindow(0, overlay.x(), overlay.y(), overlay.width(), overlay.height())
            .save(QCoreApplication::applicationDirPath() + QStringLiteral("/artifacts/bench-screen.png"));
        timed(QStringLiteral("hide"), [&] { overlay.hide(); });
        timed(QStringLiteral("second show"), [&] { overlay.show(); });
        overlay.hide();
        // The real open path: find the destination and its caret, load, position,
        // show. Hidden again at once so its temporary hotkeys exist for an instant.
        for (int i = 0; i < 3; ++i) {
            clock.start();
            overlay.showAtCursor();
            QCoreApplication::processEvents();
            const double ms = clock.nsecsElapsed() / 1e6;
            overlay.hide();
            out << QStringLiteral("%1 ms  open with hotkey path\n").arg(ms, 8, 'f', 1);
            settle();
        }
        out << QStringLiteral("rows %1  devicePixelRatio %2\n").arg(list->model()->rowCount()).arg(overlay.devicePixelRatioF());
        return 0;
    }
    // Read-only: reports where the caret of the foreground app is found. With a
    // window title after --caret, that window is brought forward first.
    if (app.arguments().contains(QStringLiteral("--caret"))) {
        QFile report(QCoreApplication::applicationDirPath() + QStringLiteral("/artifacts/caret.txt"));
        if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) return 1;
        QTextStream out(&report);
        static QString wanted;
        wanted = app.arguments().value(app.arguments().indexOf(QStringLiteral("--caret")) + 1);
        const auto titleOf = [](HWND window) {
            wchar_t text[256] = {};
            ::GetWindowTextW(window, text, 256);
            return QString::fromWCharArray(text);
        };
        const HWND before = ::GetForegroundWindow();
        HWND named = nullptr;
        if (!wanted.isEmpty())
            ::EnumWindows([](HWND top, LPARAM found) -> BOOL {
                wchar_t text[256] = {};
                ::GetWindowTextW(top, text, 256);
                if (!::IsWindowVisible(top) || !QString::fromWCharArray(text).contains(wanted)) return TRUE;
                *reinterpret_cast<HWND*>(found) = top;
                return FALSE;
            }, reinterpret_cast<LPARAM>(&named));
        if (named) { ::SetForegroundWindow(named); QTest::qWait(900); }
        platform::prepareCaretLookup();
        for (int attempt = 1; attempt <= 3; ++attempt) {
            QElapsedTimer clock; clock.start();
            const auto target = platform::capturePasteTarget();
            const QRect caret = platform::caretRect(target);
            const double ms = clock.nsecsElapsed() / 1e6;
            RECT frame{};
            ::GetWindowRect(reinterpret_cast<HWND>(target.window), &frame);
            wchar_t cls[128] = {};
            ::GetClassNameW(reinterpret_cast<HWND>(target.window), cls, 128);
            out << "attempt " << attempt << ": window \"" << titleOf(reinterpret_cast<HWND>(target.window)).left(40)
                << "\" class " << QString::fromWCharArray(cls)
                << " rect " << frame.left << "," << frame.top << "-" << frame.right << "," << frame.bottom << "\n"
                << "  caret " << (caret.isEmpty() ? QStringLiteral("not reported (popup opens at the mouse pointer)")
                       : QStringLiteral("%1,%2 %3x%4").arg(caret.x()).arg(caret.y()).arg(caret.width()).arg(caret.height()))
                << "  lookup " << ms << " ms\n";
            QTest::qWait(400);
        }
        if (named && ::IsWindow(before)) ::SetForegroundWindow(before);
        return 0;
    }
    if (app.arguments().contains(QStringLiteral("--verify"))) {
        WindowsPasteTests tests;
        return QTest::qExec(&tests, QStringList{app.applicationFilePath(), QStringLiteral("-o"),
            QCoreApplication::applicationDirPath() + QStringLiteral("/artifacts/windows-paste-results.xml,xml")});
    }
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    if (app.arguments().contains(QStringLiteral("--editor"))) {
        window.setWindowTitle(QStringLiteral("ClipStream scratch editor"));
        layout->addWidget(new QLabel(QStringLiteral("Select text, open Ctrl+Shift+V, then paste a clip.")));
        auto* edit = new QPlainTextEdit(&window);
        edit->setAccessibleName(QStringLiteral("Scratch text"));
        layout->addWidget(edit);
        auto reset = [edit] {
            edit->setPlainText(QStringLiteral("Before REPLACE ME after"));
            auto cursor = edit->textCursor();
            cursor.setPosition(7); cursor.setPosition(17, QTextCursor::KeepAnchor);
            edit->setTextCursor(cursor); edit->setFocus();
        };
        auto* button = new QPushButton(QStringLiteral("Reset selection"), &window);
        QObject::connect(button, &QPushButton::clicked, &window, reset);
        layout->addWidget(button);
        window.resize(650, 270); window.move(100, 100); window.show(); reset();
        return app.exec();
    }
    QTemporaryDir directory;
    Database db;
    if (!db.open(directory.path())) return 1;
    ClipEntry entry;
    entry.content = QStringLiteral("ClipStream pasted correctly");
    entry.sourceApp = QStringLiteral("Desktop test");
    db.insertEntry(entry);
    OverlayWindow overlay(&db, nullptr);
    HotkeyManager hotkey;
    if (!hotkey.registerHotkey(1, Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_V)) return 2;
    QObject::connect(&hotkey, &HotkeyManager::activated, &overlay, [&] { overlay.toggleAtCursor(); });
    auto* status = new QLabel(QStringLiteral("Ready. Test history is temporary."), &window);
    layout->addWidget(status);
    QObject::connect(&overlay, &OverlayWindow::pasteFailed, status, [status] { status->setText(QStringLiteral("Paste failed")); });
    auto* quit = new QPushButton(QStringLiteral("Quit desktop test"), &window);
    layout->addWidget(quit);
    QObject::connect(quit, &QPushButton::clicked, &app, &QApplication::quit);
    window.setWindowTitle(QStringLiteral("ClipStream desktop test"));
    window.resize(360, 100); window.move(100, 450); window.show();
    QProcess editor;
    editor.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--editor")});
    const int result = app.exec();
    editor.terminate(); editor.waitForFinished(2000);
    return result;
}

#include "DesktopCheck.moc"
