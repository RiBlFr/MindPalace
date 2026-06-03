#include "ScheduleCalendarDialog.h"
#include <QVBoxLayout>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QMetaObject>

// =========================================================
// CustomCalendarWidget 实现
// =========================================================

CustomCalendarWidget::CustomCalendarWidget(QWidget *parent) : QCalendarWidget(parent) {}

void CustomCalendarWidget::setScheduleData(const QMap<QDate, QStringList>& data) {
    m_scheduleData = data;
    updateCells(); // 强制整个日历控件重新绘制一遍
}

void CustomCalendarWidget::paintCell(QPainter *painter, const QRect &rect, const QDate date) const {
    // 1. 先让父类把原本灰白底色、日期数字画好
    QCalendarWidget::paintCell(painter, rect, date);

    // 2. 拦截检查：这天有没有要复习的卡组？
    if (m_scheduleData.contains(date)) {
        const QStringList& decks = m_scheduleData[date];
        if (decks.isEmpty()) return;

        painter->save(); // 保护画笔状态
        
        // 设置一支蓝色的画笔用来写任务名称
        painter->setPen(QColor("#4A90E2")); 
        QFont font = painter->font();
        font.setPointSize(8); // 字号小一点，避免超出格子
        painter->setFont(font);

        // 如果只有1个卡组，直接写名字；如果多个，写 "C++等 3项"
        QString text = decks.size() == 1 ? decks.first() : QString("%1等 %2项").arg(decks.first()).arg(decks.size());
        
        // 精确控制绘制位置：让文字显示在方格的底部区域
        QRect textRect = rect;
        textRect.setTop(rect.bottom() - 18); 
        
        painter->drawText(textRect, Qt::AlignCenter, text);
        
        painter->restore(); // 归还画笔状态
    }
}

// =========================================================
// ScheduleCalendarDialog 实现
// =========================================================

ScheduleCalendarDialog::ScheduleCalendarDialog(const QStringList& availableDecks, QWidget *parent)
    : QDialog(parent), m_availableDecks(availableDecks) 
{
    setWindowTitle(tr("复习计划看板"));
    resize(800, 600);

    auto *layout = new QVBoxLayout(this);
    m_calendar = new CustomCalendarWidget(this);
    
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
    // 呼叫主界面 -> 呼叫总控 -> 总控通过引用把 data 填满
    emit signal_requestCalendarData(m_calendar->yearShown(), m_calendar->monthShown(), data);
    m_calendar->setScheduleData(data); // 喂给底层绘图组件
}

void ScheduleCalendarDialog::onCustomContextMenuRequested(const QPoint &pos) {
    if (m_availableDecks.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("当前没有任何卡组，无法安排计划。"));
        return;
    }

    // 获取用户选中的是哪一天
    QDate selectedDate = m_calendar->selectedDate();

    // 弹出优美的右键操作菜单
    QMenu menu(this);
    QAction *sprintAct = menu.addAction(tr("💪 安排强制复习 (冲刺)"));
    QAction *restAct   = menu.addAction(tr("🏖️ 安排强制休假 (休息)"));
    menu.addSeparator();
    QAction *clearAct  = menu.addAction(tr("🔄 清除该日自定义计划 (恢复算法)"));

    QAction *result = menu.exec(m_calendar->mapToGlobal(pos));
    if (!result) return; // 用户点空白处取消了

    int status = 0;
    if (result == sprintAct) status = 1;
    else if (result == restAct) status = -1;
    else if (result == clearAct) status = 0;

    // 弹出二次确认，问用户要对哪个卡组下手
    bool ok;
    QString targetDeck = QInputDialog::getItem(this, tr("选择目标卡组"), 
                                               tr("请选择要在 %1 调整进度的卡组：").arg(selectedDate.toString("MM-dd")), 
                                               m_availableDecks, 0, false, &ok);
    if (ok && !targetDeck.isEmpty()) {
        // 抛出修改信号
        emit signal_requestUpdateSchedule(targetDeck, selectedDate, status);
        // 修改完硬盘后，立刻重绘今天的日历画面
        refreshCalendarData();
    }
}