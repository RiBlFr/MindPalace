#ifndef DESKTOPPETWIDGET_H
#define DESKTOPPETWIDGET_H

#include <QWidget>

namespace MindPalace::DesktopPet {

bool isEnabled();
void setEnabled(bool enabled);
void syncStartup(bool enabled);
void startProcess();
void ensureRunningIfEnabled();
int runDesktopPetMode();

} // namespace MindPalace::DesktopPet

class DesktopPetWidget final : public QWidget {
public:
    explicit DesktopPetWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void placeNearBottomRight();
    void advanceFrame();
    void applyScale(qreal scale);
    void updateWindowMask();
    QString todayReviewSummary() const;

    QPixmap m_sprite;
    QPoint m_dragStartGlobal;
    QPoint m_dragStartTopLeft;
    int m_frameIndex = 0;
    qreal m_scale = 1.0;
    bool m_dragging = false;
    bool m_movedDuringPress = false;
};

#endif // DESKTOPPETWIDGET_H
