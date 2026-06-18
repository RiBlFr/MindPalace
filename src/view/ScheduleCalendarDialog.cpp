#include "ScheduleCalendarDialog.h"
#include <QVBoxLayout>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QMetaObject>
#include <QLabel>
#include <QFile>
#include <QTextCharFormat>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#endif

#include "StyledDialogs.h"

namespace {
#ifdef Q_OS_WIN
void applyCalendarWindowFrame(QWidget *widget, bool dark) {
    if (!widget) return;

    const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    const BOOL darkMode = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));

    const COLORREF caption = dark ? RGB(5, 9, 16) : RGB(247, 248, 251);
    const COLORREF text = dark ? RGB(232, 238, 248) : RGB(31, 41, 55);
    DwmSetWindowAttribute(hwnd, 35, &caption, sizeof(caption));
    DwmSetWindowAttribute(hwnd, 36, &text, sizeof(text));
}
#else
void applyCalendarWindowFrame(QWidget*, bool) {}
#endif
}

// Calendar cell painting

CustomCalendarWidget::CustomCalendarWidget(QWidget *parent) : QCalendarWidget(parent) {}

void CustomCalendarWidget::setScheduleData(const QMap<QDate, QStringList>& data) {
    m_scheduleData = data;
    updateCells(); // 强制整个日历控件重新绘制一遍
}

void CustomCalendarWidget::setCheckInDates(const QSet<QDate>& dates) {
    m_checkInDates = dates;
    updateCells();
}

void CustomCalendarWidget::setDarkMode(bool darkMode) {
    m_darkMode = darkMode;
    updateCells();
}

void CustomCalendarWidget::paintCell(QPainter *painter, const QRect &rect, const QDate date) const {
    // 完全自绘格子，换取圆角选中态、今日描边与排期药丸徽章的精细控制。
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool isSelected     = (date == selectedDate());
    const bool isToday        = (date == QDate::currentDate());
    const bool inCurrentMonth = (date.month() == monthShown() && date.year() == yearShown());
    const bool isWeekend      = (date.dayOfWeek() == Qt::Saturday || date.dayOfWeek() == Qt::Sunday);
    const bool hasSchedule    = m_scheduleData.contains(date) && !m_scheduleData[date].isEmpty();
    const bool isCheckedIn    = m_checkInDates.contains(date);

    const QRect cellRect = rect.adjusted(m_darkMode ? 4 : 3, m_darkMode ? 4 : 3,
                                         m_darkMode ? -4 : -3, m_darkMode ? -4 : -3);

    // 1. 背景：选中填充实色，今天描边
    if (isSelected) {
        if (m_darkMode) {
            painter->setPen(QPen(QColor("#2a95ff"), 1.3));
            painter->setBrush(QColor(14, 45, 88, 205));
            painter->drawRoundedRect(cellRect, 8, 8);
            painter->setPen(QPen(QColor(42, 149, 255, 70), 6));
            painter->drawRoundedRect(cellRect.adjusted(2, 2, -2, -2), 8, 8);
        } else {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor("#5688b8"));
            painter->drawRoundedRect(cellRect, 8, 8);
        }
    } else if (isToday) {
        painter->setPen(QPen(m_darkMode ? QColor("#2a95ff") : QColor("#5688b8"), 1.4));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(cellRect, 8, 8);
    }

    // 2. 日期数字
    QColor dayColor;
    if (m_darkMode) {
        if (isSelected)            dayColor = QColor("#ffffff");
        else if (!inCurrentMonth)  dayColor = QColor("#626b79");
        else if (isWeekend)        dayColor = QColor("#ff5a60");
        else                       dayColor = QColor("#e8eef8");
    } else {
        if (isSelected)            dayColor = QColor("#ffffff");
        else if (!inCurrentMonth)  dayColor = QColor("#cbd2d9");
        else if (isWeekend)        dayColor = QColor("#cd7373");
        else                       dayColor = QColor("#334155");
    }

    QFont dayFont = painter->font();
    dayFont.setPointSize(m_darkMode ? 11 : 10);
    dayFont.setBold(isToday || isSelected);
    painter->setFont(dayFont);
    painter->setPen(dayColor);

    const QRect numberRect(rect.left(), rect.top() + 6, rect.width(), 26);
    painter->drawText(numberRect, Qt::AlignCenter, QString::number(date.day()));

    if (isCheckedIn && inCurrentMonth) {
        QFont badgeFont = painter->font();
        badgeFont.setPointSize(7);
        badgeFont.setBold(true);
        painter->setFont(badgeFont);

        const int badgeWidth = qMin(rect.width() - 14, m_darkMode ? 72 : 58);
        const int badgeHeight = 18;
        const int badgeX = rect.center().x() - badgeWidth / 2;
        const int preferredBadgeY = rect.top() + 42;
        const int scheduleBadgeY = rect.bottom() - 19;
        const int badgeY = qMin(preferredBadgeY, scheduleBadgeY - badgeHeight - 8);
        const QRect badgeRect(badgeX, badgeY, badgeWidth, badgeHeight);

        painter->setPen(m_darkMode ? QPen(QColor("#30f29b"), 1.0) : QPen(Qt::NoPen));
        painter->setBrush(isSelected ? QColor("#16c879") : QColor("#32c67a"));
        painter->drawRoundedRect(badgeRect, badgeHeight / 2, badgeHeight / 2);

        const QRect checkRect(badgeRect.left() + 6, badgeRect.top() + 4, 10, 10);
        painter->setBrush(QColor(255, 255, 255, 215));
        painter->drawEllipse(checkRect);

        QPen checkPen(isSelected ? QColor("#2ab96f") : QColor("#23a968"),
                      1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(checkPen);
        painter->drawLine(checkRect.left() + 3, checkRect.center().y(),
                          checkRect.left() + 5, checkRect.bottom() - 3);
        painter->drawLine(checkRect.left() + 5, checkRect.bottom() - 3,
                          checkRect.right() - 2, checkRect.top() + 3);

        painter->setPen(QColor(m_darkMode ? "#d9ffe9" : "#ffffff"));
        painter->drawText(badgeRect.adjusted(18, 0, -5, 0),
                          Qt::AlignVCenter | Qt::AlignLeft,
                          QStringLiteral("已签到"));
    }

    // 3. 排期药丸徽章
    if (hasSchedule) {
        const QStringList& decks = m_scheduleData[date];
        const QString text = decks.size() == 1
            ? decks.first()
            : QString("%1 等%2项").arg(decks.first()).arg(decks.size());

        QFont badgeFont = painter->font();
        badgeFont.setPointSize(7);
        badgeFont.setBold(false);
        painter->setFont(badgeFont);

        const QRect badgeRect(rect.left() + 5, rect.bottom() - 19, rect.width() - 10, 16);
        const QColor badgeBg   = m_darkMode
                ? QColor(23, 64, 113, isSelected ? 165 : 130)
                : (isSelected ? QColor(255, 255, 255, 70) : QColor("#e8eef5"));
        const QColor badgeText = m_darkMode
                ? QColor("#bfe4ff")
                : (isSelected ? QColor("#ffffff") : QColor("#487aaa"));

        painter->setPen(m_darkMode ? QPen(QColor("#4aa3ff"), 1.0) : QPen(Qt::NoPen));
        painter->setBrush(badgeBg);
        painter->drawRoundedRect(badgeRect, 8, 8);

        painter->setPen(badgeText);
        const QString elided = painter->fontMetrics().elidedText(
            text, Qt::ElideRight, badgeRect.width() - 8);
        painter->drawText(badgeRect, Qt::AlignCenter, elided);
    }

    painter->restore();
}

// Dialog setup

ScheduleCalendarDialog::ScheduleCalendarDialog(const QStringList& availableDecks, QWidget *parent)
    : QDialog(parent), m_availableDecks(availableDecks) 
{
    setWindowTitle(tr("复习计划看板"));
    const bool darkMode = parent && parent->property("frostedCard").toBool();
    applyCalendarWindowFrame(this, darkMode);
    setObjectName("scheduleCalendar");
    setProperty("theme", darkMode ? "dark" : "light");
    resize(darkMode ? QSize(1120, 760) : QSize(820, 660));
    setMinimumSize(darkMode ? QSize(920, 640) : QSize(760, 560));

    // 加载美化样式表
    QFile qssFile(":/styles/Calendar.qss");
    if (qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(QString::fromUtf8(qssFile.readAll()));
    }

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 22);
    layout->setSpacing(12);

    auto *title = new QLabel(tr("复习计划看板"), this);
    title->setObjectName("calendarTitle");
    layout->addWidget(title);

    auto *hint = new QLabel(
        tr("右键点击任意日期，可安排强制复习 / 强制休假，或清除自定义计划恢复算法默认。"), this);
    hint->setObjectName("calendarHint");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    layout->addSpacing(4);

    m_calendar = new CustomCalendarWidget(this);
    m_calendar->setObjectName("calendarView");
    m_calendar->setProperty("theme", darkMode ? "dark" : "light");
    m_calendar->setDarkMode(darkMode);
    m_calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    m_calendar->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
    m_calendar->setFirstDayOfWeek(Qt::Monday);
    m_calendar->setGridVisible(false);
    m_calendar->setNavigationBarVisible(true);

    if (darkMode) {
        QTextCharFormat weekdayFormat;
        weekdayFormat.setForeground(QBrush(QColor("#dce6f5")));
        weekdayFormat.setFontWeight(QFont::DemiBold);
        QTextCharFormat weekendFormat = weekdayFormat;
        weekendFormat.setForeground(QBrush(QColor("#ff454b")));
        for (int day = Qt::Monday; day <= Qt::Friday; ++day) {
            m_calendar->setWeekdayTextFormat(static_cast<Qt::DayOfWeek>(day), weekdayFormat);
        }
        m_calendar->setWeekdayTextFormat(Qt::Saturday, weekendFormat);
        m_calendar->setWeekdayTextFormat(Qt::Sunday, weekendFormat);
    }

    // 开启自定义右键菜单支持
    m_calendar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_calendar, &QWidget::customContextMenuRequested,
            this, &ScheduleCalendarDialog::onCustomContextMenuRequested);

    // 当用户翻页（看上个月/下个月）时，主动拉取新数据
    connect(m_calendar, &QCalendarWidget::currentPageChanged,
            this, [this](int, int){ refreshCalendarData(); });

    layout->addWidget(m_calendar);

    // 首次打开弹窗时，延迟一丢丢加载数据，确保外部的信号槽都已经连接完毕
    QMetaObject::invokeMethod(this, "refreshCalendarData", Qt::QueuedConnection);
}

void ScheduleCalendarDialog::refreshCalendarData() {
    QMap<QDate, QStringList> data;
    QSet<QDate> checkInDates;
    // 呼叫主界面 -> 呼叫总控 -> 总控通过引用把 data 填满
    emit signal_requestCalendarData(m_calendar->yearShown(), m_calendar->monthShown(), data);
    emit signal_requestCheckInDates(m_calendar->yearShown(), m_calendar->monthShown(), checkInDates);
    m_calendar->setScheduleData(data); // 喂给底层绘图组件
    m_calendar->setCheckInDates(checkInDates);
}

void ScheduleCalendarDialog::onCustomContextMenuRequested(const QPoint &pos) {
    if (m_availableDecks.isEmpty()) {
        StyledDialogs::info(this, tr("提示"), tr("当前没有任何卡组，无法安排计划。"));
        return;
    }

    // 获取用户选中的是哪一天
    QDate selectedDate = m_calendar->selectedDate();

    // 弹出优美的右键操作菜单
    QMenu menu(this);
    QAction *sprintAct = menu.addAction(tr("安排强制复习"));
    QAction *restAct   = menu.addAction(tr("安排休假"));
    menu.addSeparator();
    QAction *clearAct  = menu.addAction(tr("🔄 清除该日自定义计划"));

    QAction *result = menu.exec(m_calendar->mapToGlobal(pos));
    if (!result) return; // 用户点空白处取消了

    int status = 0;
    if (result == sprintAct) status = 1;
    else if (result == restAct) status = -1;
    else if (result == clearAct) status = 0;

    // 弹出二次确认，问用户要对哪个卡组下手（套用 SimpleDialog 统一风格）
    QInputDialog inputDlg(this);
    StyledDialogs::applyStyle(&inputDlg);
    inputDlg.setWindowTitle(tr("选择目标卡组"));
    inputDlg.setLabelText(tr("请选择要在 %1 调整进度的卡组：").arg(selectedDate.toString("MM-dd")));
    inputDlg.setComboBoxItems(m_availableDecks);
    inputDlg.setComboBoxEditable(false);

    const bool ok = (inputDlg.exec() == QDialog::Accepted);
    const QString targetDeck = inputDlg.textValue();
    if (ok && !targetDeck.isEmpty()) {
        // 抛出修改信号
        emit signal_requestUpdateSchedule(targetDeck, selectedDate, status);
        // 修改完硬盘后，立刻重绘今天的日历画面
        refreshCalendarData();
    }
}
