#ifndef MINDPALACE_SCHEDULECALENDARDIALOG_H
#define MINDPALACE_SCHEDULECALENDARDIALOG_H

#include <QDialog>
#include <QCalendarWidget>
#include <QMap>
#include <QDate>
#include <QSet>
#include <QStringList>

// Calendar widget that paints review and sign-in markers inside date cells.
class CustomCalendarWidget : public QCalendarWidget {
    Q_OBJECT
public:
    explicit CustomCalendarWidget(QWidget *parent = nullptr);

    void setScheduleData(const QMap<QDate, QStringList>& data);
    void setCheckInDates(const QSet<QDate>& dates);
    void setDarkMode(bool darkMode);

protected:
    void paintCell(QPainter *painter, const QRect &rect, const QDate date) const override;

private:
    QMap<QDate, QStringList> m_scheduleData;
    QSet<QDate> m_checkInDates;
    bool m_darkMode = false;
};

// Dialog wrapper for calendar layout, context menus, and data requests.
class ScheduleCalendarDialog : public QDialog {
    Q_OBJECT
public:
    // 构造时传入系统现有的牌组列表，供右键菜单选择
    explicit ScheduleCalendarDialog(const QStringList& availableDecks, QWidget *parent = nullptr);

    signals:
        // The output references are filled synchronously by AppController.
        void signal_requestCalendarData(int year, int month, QMap<QDate, QStringList>& outData);
        void signal_requestCheckInDates(int year, int month, QSet<QDate>& outDates);

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
