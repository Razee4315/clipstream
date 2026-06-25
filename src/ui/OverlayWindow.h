#pragma once

#include <QWidget>

// The frameless, translucent launcher-style overlay. Phase 0 shows a placeholder
// card; the history list, search and actions land in later phases. It positions
// itself on the cursor's screen (multi-monitor aware) and hides on focus loss.
class OverlayWindow : public QWidget {
    Q_OBJECT
public:
    explicit OverlayWindow(QWidget* parent = nullptr);

    void showAtCursor();   // pop up near the cursor, on its current screen
    void toggleAtCursor(); // show if hidden, hide if visible

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void buildUi();

    QWidget* m_card = nullptr;
};
