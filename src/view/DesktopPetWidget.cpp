#include "DesktopPetWidget.h"

#include "controller/deckcontroller.h"

#include <QAction>
#include <QApplication>
#include <QBitmap>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QGuiApplication>
#include <QLockFile>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QWheelEvent>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#endif

namespace {
constexpr int kFrameWidth = 192;
constexpr int kFrameHeight = 208;
constexpr int kFrameCount = 6;
constexpr int kFrameIntervalMs = 1100 / kFrameCount;
constexpr qreal kMinScale = 0.45;
constexpr qreal kMaxScale = 2.2;

QString lockPath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return QDir(dir).filePath(QStringLiteral("MindPalaceDesktopPet.lock"));
}

QString startupCommand() {
    const QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    return QString("\"%1\" --desktop-pet").arg(appPath);
}

QString decksDirPath() {
    const QString relPath = QStringLiteral("data/decks");
    const QString appDirPath = QCoreApplication::applicationDirPath() + "/" + relPath;
    return QDir(appDirPath).exists() ? appDirPath : relPath;
}

#ifdef Q_OS_WIN
void removeNativeWindowFrame(QWidget *widget) {
    if (!widget) return;

    const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    const int disabledNcRendering = 2; // DWMNCRP_DISABLED
    DwmSetWindowAttribute(hwnd, 2, &disabledNcRendering, sizeof(disabledNcRendering));

    const int noCorners = 1; // DWMWCP_DONOTROUND
    DwmSetWindowAttribute(hwnd, 33, &noCorners, sizeof(noCorners));
}
#else
void removeNativeWindowFrame(QWidget*) {}
#endif
} // namespace

namespace MindPalace::DesktopPet {

bool isEnabled() {
    return QSettings("MindPalace", "Settings")
            .value("desktopPetEnabled", false)
            .toBool();
}

void syncStartup(bool enabled) {
#ifdef Q_OS_WIN
    QSettings runKey(
            "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            QSettings::NativeFormat);
    if (enabled) {
        runKey.setValue("MindPalaceDesktopPet", startupCommand());
    } else {
        runKey.remove("MindPalaceDesktopPet");
    }
#else
    Q_UNUSED(enabled);
#endif
}

void startProcess() {
    QProcess::startDetached(QCoreApplication::applicationFilePath(),
                            QStringList{QStringLiteral("--desktop-pet")});
}

void setEnabled(bool enabled) {
    QSettings settings("MindPalace", "Settings");
    settings.setValue("desktopPetEnabled", enabled);
    syncStartup(enabled);
    if (enabled) {
        startProcess();
    }
}

void ensureRunningIfEnabled() {
    syncStartup(isEnabled());
    if (isEnabled()) {
        startProcess();
    }
}

int runDesktopPetMode() {
    QLockFile lock(lockPath());
    if (!lock.tryLock(100)) {
        return 0;
    }

    if (!isEnabled()) {
        return 0;
    }

    DesktopPetWidget pet;
    pet.show();
    return QApplication::exec();
}

} // namespace MindPalace::DesktopPet

DesktopPetWidget::DesktopPetWidget(QWidget *parent)
        : QWidget(parent),
          m_sprite(QStringLiteral(":/pets/pet_002_idle.png")) {
    setWindowFlags(Qt::FramelessWindowHint
                   | Qt::Tool
                   | Qt::WindowStaysOnTopHint
                   | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAutoFillBackground(false);
    setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    setCursor(Qt::PointingHandCursor);

    applyScale(QSettings("MindPalace", "Settings")
                       .value("desktopPetScale", 1.0)
                       .toDouble());
    placeNearBottomRight();
    removeNativeWindowFrame(this);
    updateWindowMask();

    auto *frameTimer = new QTimer(this);
    connect(frameTimer, &QTimer::timeout, this, [this]() {
        advanceFrame();
    });
    frameTimer->start(kFrameIntervalMs);

    auto *settingsTimer = new QTimer(this);
    connect(settingsTimer, &QTimer::timeout, this, []() {
        if (!MindPalace::DesktopPet::isEnabled()) {
            QApplication::quit();
        }
    });
    settingsTimer->start(1000);
}

void DesktopPetWidget::placeNearBottomRight() {
    const QScreen *screen = QGuiApplication::primaryScreen();
    const QRect area = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 720);
    move(area.right() - width() - 36, area.bottom() - height() - 28);
}

void DesktopPetWidget::advanceFrame() {
    m_frameIndex = (m_frameIndex + 1) % kFrameCount;
    updateWindowMask();
    update();
}

void DesktopPetWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    if (!m_sprite.isNull()) {
        const QRect source(m_frameIndex * kFrameWidth, 0, kFrameWidth, kFrameHeight);
        painter.drawPixmap(rect(), m_sprite, source);
    }
}

void DesktopPetWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_movedDuringPress = false;
        m_dragStartGlobal = event->globalPosition().toPoint();
        m_dragStartTopLeft = frameGeometry().topLeft();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void DesktopPetWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging) {
        const QPoint delta = event->globalPosition().toPoint() - m_dragStartGlobal;
        if (delta.manhattanLength() > 3) {
            m_movedDuringPress = true;
        }
        move(m_dragStartTopLeft + delta);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void DesktopPetWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        if (!m_movedDuringPress) {
            QProcess::startDetached(QCoreApplication::applicationFilePath(), QStringList{});
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void DesktopPetWidget::wheelEvent(QWheelEvent *event) {
    const qreal step = event->angleDelta().y() > 0 ? 0.08 : -0.08;
    const QPoint centerBefore = geometry().center();
    applyScale(m_scale + step);
    move(centerBefore - QPoint(width() / 2, height() / 2));
    QSettings("MindPalace", "Settings").setValue("desktopPetScale", m_scale);
    updateWindowMask();
    update();
    event->accept();
}

void DesktopPetWidget::contextMenuEvent(QContextMenuEvent *event) {
    QMenu menu(this);
    QAction *todayAction = menu.addAction(QStringLiteral("查看今日待复习"));
    menu.addSeparator();
    QAction *disableAction = menu.addAction(QStringLiteral("取消桌面宠物"));
    QAction *selected = menu.exec(event->globalPos());
    if (selected == todayAction) {
        QMessageBox::information(this, QStringLiteral("今日待复习"), todayReviewSummary());
    } else if (selected == disableAction) {
        MindPalace::DesktopPet::setEnabled(false);
        QApplication::quit();
    }
}

void DesktopPetWidget::applyScale(qreal scale) {
    m_scale = qBound(kMinScale, scale, kMaxScale);
    setFixedSize(qRound(kFrameWidth * m_scale), qRound(kFrameHeight * m_scale));
}

void DesktopPetWidget::updateWindowMask() {
    if (m_sprite.isNull() || width() <= 0 || height() <= 0) return;

    const QRect source(m_frameIndex * kFrameWidth, 0, kFrameWidth, kFrameHeight);
    const QPixmap frame = m_sprite.copy(source).scaled(size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
    const QImage alpha = frame.toImage().convertToFormat(QImage::Format_ARGB32);
    setMask(QBitmap::fromImage(alpha.createAlphaMask(Qt::ThresholdDither)));
}

QString DesktopPetWidget::todayReviewSummary() const {
    MindPalace::Controller::DeckController controller(decksDirPath());
    QStringList lines;
    int totalCards = 0;

    const QString todayKey = QDate::currentDate().toString("yyyy-MM-dd");
    for (const auto& deck : controller.getDecks()) {
        const int manualStatus = deck.manualSchedule.value(todayKey, 0);
        if (manualStatus == -1) {
            continue;
        }

        const int dueCount = manualStatus == 1
                ? static_cast<int>(deck.cards.size())
                : deck.getDueCount();
        if (dueCount <= 0) {
            continue;
        }

        totalCards += dueCount;
        lines << QStringLiteral("%1：%2 张").arg(deck.deckName).arg(dueCount);
    }

    if (totalCards <= 0) {
        return QStringLiteral("今天没有待复习卡片。");
    }

    return QStringLiteral("今日待复习牌组：\n\n%1\n\n共 %2 张卡片。")
            .arg(lines.join('\n'))
            .arg(totalCards);
}
