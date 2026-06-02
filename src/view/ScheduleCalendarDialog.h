#ifndef MINDPALACE_SCHEDULECALENDARDIALOG_H
#define MINDPALACE_SCHEDULECALENDARDIALOG_H

#include <QDialog>
#include <QCalendarWidget>
#include <QMap>
#include <QDate>
#include <QStringList>

// =========================================================
// 1. 核心渲染类：接管画笔，在日历格子里绘制我们自己的业务数据
// =========================================================
class CustomCalendarWidget : public QCalendarWidget {
    Q_OBJECT
public:
    explicit CustomCalendarWidget(QWidget *parent = nullptr);

    // 接收外界传来的排期数据
    void setScheduleData(const QMap<QDate, QStringList>& data);

protected:
    // 【核心重写】每次界面刷新时，Qt 会调用这个函数来画每一个日期格子
    void paintCell(QPainter *painter, const QRect &rect, const QDate date) const override;

private:
    QMap<QDate, QStringList> m_scheduleData;
};

// =========================================================
// 2. 弹窗面板类：负责整体布局、右键菜单交互以及与外界通信
// =========================================================
class ScheduleCalendarDialog : public QDialog {
    Q_OBJECT
public:
    // 构造时传入系统现有的牌组列表，供右键菜单选择
    explicit ScheduleCalendarDialog(const QStringList& availableDecks, QWidget *parent = nullptr);

    signals:
        // 【高阶技巧】注意最后的 outData 是一个引用(&)！
        // 这样当弹窗发出请求时，AppController 可以瞬间把数据填进这个变量里，实现同步获取。
        void signal_requestCalendarData(int year, int month, QMap<QDate, QStringList>& outData);

    // 向外抛出修改计划的请求
    void signal_requestUpdateSchedule(const QString& deckName, const QDate& date, int status);

private slots:
    void onCustomContextMenuRequested(const QPoint &pos);
    void refreshCalendarData();

private:
    CustomCalendarWidget *m_calendar;
    QStringList m_availableDecks;
};

#endif // MINDPALACE_SCHEDULECALENDARDIALOG_H