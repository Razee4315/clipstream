#include "core/Database.h"
#include "core/ClipboardMonitor.h"
#include "core/ContentClassifier.h"
#include "ui/OverlayWindow.h"
#include "ui/HistoryModel.h"
#include "theme.h"

#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDir>
#include <QFontDatabase>
#include <QLabel>
#include <QInputDialog>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <memory>

class ClipStreamTests : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    std::unique_ptr<Database> db;

    qint64 add(const QString& text, ContentType type = ContentType::Text,
               const QString& source = QStringLiteral("Notes.exe"), bool sensitive = false) {
        ClipEntry entry;
        entry.content = text;
        entry.type = type;
        entry.sourceApp = source;
        entry.sensitive = sensitive;
        return db->insertEntry(entry);
    }

private slots:
    void initTestCase() {
#ifdef Q_OS_WIN
        // The offscreen plugin does not discover Windows fonts on its own.
        const QString fonts = qEnvironmentVariable("WINDIR") + QStringLiteral("/Fonts/");
        QVERIFY(QFontDatabase::addApplicationFont(fonts + QStringLiteral("segoeui.ttf")) >= 0);
        QVERIFY(QFontDatabase::addApplicationFont(fonts + QStringLiteral("segoeuib.ttf")) >= 0);
#endif
        QFont font(Theme::fontFamily());
        font.setPixelSize(Theme::FsBody);
        QApplication::setFont(font);
    }
    void init() {
        QVERIFY(directory.isValid());
        db = std::make_unique<Database>();
        QVERIFY(db->open(directory.path()));
        db->clearHistory();
        Theme::setThemeId(QStringLiteral("dark"));
    }
    void cleanup() { db.reset(); }

    void literalSearch() {
        add(QStringLiteral("https://example.com/design?q=hello"), ContentType::Url);
        add(QStringLiteral("C:\\Users\\design_notes"));
        add(QStringLiteral("Save 50% today"));
        add(QStringLiteral("He said \"hello\""));
        add(QStringLiteral("Ordinary content"), ContentType::Text, QStringLiteral("Figma.exe"));
        for (const QString& query : {QStringLiteral("https://example.com"), QStringLiteral("design_notes"),
             QStringLiteral("50%"), QStringLiteral("\"hello\""), QStringLiteral("Figma"), QStringLiteral("C:\\Users")}) {
            QVERIFY2(!db->search(query).isEmpty(), qPrintable(query));
        }
        QCOMPARE(db->search(QStringLiteral("%" )).size(), 1);
        QCOMPARE(db->search(QStringLiteral("_" )).size(), 1);
        QVERIFY(db->search(QStringLiteral("NOT missing OR nonexistent")).isEmpty());
        QVERIFY(db->search(QStringLiteral("unmatched ( bracket")).isEmpty());
    }

    void filtersApplyBeforeLimit() {
        add(QStringLiteral("An old image"), ContentType::Image);
        for (int i = 0; i < 205; ++i) add(QStringLiteral("Text %1").arg(i));
        QCOMPARE(db->search(QString(), 200, ClipFilter::Images).size(), 1);
        const auto link = add(QStringLiteral("https://example.com"), ContentType::Url);
        QVERIFY(db->togglePin(link));
        QCOMPARE(db->search(QString(), 200, ClipFilter::Pinned).size(), 1);
        QCOMPARE(db->search(QString(), 200, ClipFilter::Links).first().id, link);
        QVERIFY(db->search(QStringLiteral("absent"), 200, ClipFilter::Pinned).isEmpty());
    }

    void secretsAndEdits() {
        const auto id = add(QStringLiteral("hidden needle"), ContentType::Text, QStringLiteral("Vault"), true);
        QVERIFY(db->search(QStringLiteral("needle")).isEmpty());
        QVERIFY(db->search(QStringLiteral("Vault")).isEmpty());
        QVERIFY(db->updateContent(id, QStringLiteral("https://example.com")));
        QCOMPARE(db->entryById(id)->type, ContentType::Url);
        QVERIFY(!db->entryById(id)->sensitive);
        QCOMPARE(db->search(QStringLiteral("example")).size(), 1);
        const QString secret = QStringLiteral("sk-abcdefghijklmnopqrstuvwxyz1234567890ABCDEFGHIJ");
        QVERIFY(ContentClassifier::looksSensitive(secret));
        QVERIFY(db->updateContent(id, secret));
        QVERIFY(db->entryById(id)->sensitive);
        QVERIFY(db->search(QStringLiteral("abcdefghijklmnopqrstuvwxyz")).isEmpty());
        QVERIFY(db->removeEntry(id));
    }

    void retentionDoesNotCountPins() {
        for (int i = 0; i < 3; ++i) {
            const auto id = add(QStringLiteral("Pinned %1").arg(i));
            QVERIFY(db->togglePin(id));
        }
        for (int i = 0; i < 5; ++i) add(QStringLiteral("Recent %1").arg(i));
        db->cleanup(30, 2);
        QCOMPARE(db->search(QString()).size(), 5);
        QCOMPARE(db->search(QStringLiteral("Recent")).size(), 2);
    }

    void capturePreservesWhitespaceAndPauseCancelsImages() {
        ClipboardMonitor monitor;
        QSignalSpy textSpy(&monitor, &ClipboardMonitor::textCaptured);
        QSignalSpy imageSpy(&monitor, &ClipboardMonitor::imageCaptured);
        const QString text = QStringLiteral("  indented text\n\tsecond line\n");
        QApplication::clipboard()->setText(text);
        QTRY_COMPARE(textSpy.size(), 1);
        QCOMPARE(textSpy.first().first().toString(), text);
        QImage image(8, 8, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QApplication::clipboard()->setImage(image);
        monitor.setPaused(true);
        QTest::qWait(280);
        QCOMPARE(imageSpy.size(), 0);
    }

    void selectionSurvivesReloadAndPin() {
        const auto first = add(QStringLiteral("First clip"));
        add(QStringLiteral("Second clip"));
        OverlayWindow overlay(db.get(), nullptr);
        overlay.reload();
        auto* list = overlay.findChild<QListView*>();
        auto* search = overlay.findChild<QLineEdit*>(QStringLiteral("search"));
        auto* model = qobject_cast<HistoryModel*>(list->model());
        list->setCurrentIndex(model->index(1, 0));
        QCOMPARE(model->entryAt(list->currentIndex().row()).id, first);
        add(QStringLiteral("Incoming clip"));
        overlay.reload();
        QCOMPARE(model->entryAt(list->currentIndex().row()).id, first);
        QTest::keyClick(search, Qt::Key_P, Qt::ControlModifier);
        QVERIFY(db->entryById(first)->pinned);
        QCOMPARE(model->entryAt(list->currentIndex().row()).id, first);
        search->setText(QStringLiteral("Incoming"));
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(list->currentIndex().row(), 0);
    }

    void previewMasksSecrets() {
        add(QStringLiteral("Do not show this secret"), ContentType::Text, QStringLiteral("Vault"), true);
        OverlayWindow overlay(db.get(), nullptr);
        overlay.reload();
        QTimer::singleShot(0, &overlay, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog) { QFAIL("Preview did not open"); return; }
            QTimer::singleShot(1000, dialog, &QDialog::reject);
            auto* text = dialog->findChild<QPlainTextEdit*>();
            QVERIFY(text);
            QVERIFY(!text->toPlainText().contains(QStringLiteral("Do not show")));
            for (auto* button : dialog->findChildren<QPushButton*>()) {
                if (button->text() == QStringLiteral("Reveal content")) button->click();
            }
            QCOMPARE(text->toPlainText(), QStringLiteral("Do not show this secret"));
            dialog->reject();
        });
        overlay.findChild<QPushButton*>(QStringLiteral("previewButton"))->click();
    }

    void savingExistingSnippetKeepsItPinned() {
        const auto id = add(QStringLiteral("Reusable reply"));
        QVERIFY(db->togglePin(id));
        OverlayWindow overlay(db.get(), nullptr);
        overlay.showAtCursor();
        QTest::qWait(150);
        QTimer::singleShot(0, &overlay, [&] {
            auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
            if (!dialog) { QFAIL("Snippet dialog did not open"); return; }
            dialog->setTextValue(QStringLiteral("Reusable reply"));
            dialog->accept();
        });
        overlay.findChild<QToolButton*>(QStringLiteral("newSnippet"))->click();
        QVERIFY(overlay.isVisible());
        QVERIFY(db->entryById(id)->pinned);
        QCOMPARE(db->search(QString()).size(), 1);
    }

    void renderOverlayAndEmptyStates() {
        add(QStringLiteral("A little less searching. A lot more doing."), ContentType::Text, QStringLiteral("Notes"));
        add(QStringLiteral("const ideas = clipboard.filter(clip => clip.pinned);"), ContentType::Code, QStringLiteral("Code"));
        add(QStringLiteral("#3b82f6"), ContentType::Color, QStringLiteral("Figma"));
        add(QStringLiteral("https://www.figma.com/design/clipstream"), ContentType::Url, QStringLiteral("Chrome"));
        const auto pin = add(QStringLiteral("Thanks for reaching out! I’ll take a look and get back to you."), ContentType::Text, QStringLiteral("Snippet"));
        db->togglePin(pin);
        ClipboardMonitor monitor;
        OverlayWindow overlay(db.get(), &monitor);
        overlay.reload();
        overlay.showAtCursor();
        QTest::qWait(160);
        auto* actions = overlay.findChild<QWidget*>(QStringLiteral("actionsBar"));
        QVERIFY(actions->x() > overlay.width() / 2);
        QDir().mkpath(QStringLiteral("artifacts"));
        QVERIFY(overlay.grab().save(QStringLiteral("artifacts/overlay-dark.png")));
        Theme::setThemeId(QStringLiteral("light"));
        overlay.applyTheme();
        QTest::qWait(30);
        QVERIFY(overlay.grab().save(QStringLiteral("artifacts/overlay-light.png")));
        auto* search = overlay.findChild<QLineEdit*>(QStringLiteral("search"));
        search->setText(QStringLiteral("no-such-clip"));
        QTest::qWait(30);
        QVERIFY(!overlay.findChild<QPushButton*>(QStringLiteral("previewButton"))->isEnabled());
        QCOMPARE(overlay.findChild<QListView*>()->model()->rowCount(), 0);
        QVERIFY(overlay.grab().save(QStringLiteral("artifacts/overlay-empty.png")));
        auto* capture = overlay.findChild<QToolButton*>(QStringLiteral("capture"));
        capture->click();
        QVERIFY(monitor.isPaused());
        QCOMPARE(db->setting(QStringLiteral("paused")), QStringLiteral("1"));
    }
};

QTEST_MAIN(ClipStreamTests)
#include "ClipStreamTests.moc"
