#ifndef WEEKLYCHARTWIDGET_H
#define WEEKLYCHARTWIDGET_H

#include <QWidget>
#include <vector>
#include <QStringList>
#include <QVariantAnimation>

class WeeklyChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit WeeklyChartWidget(QWidget *parent = nullptr);

    // Seven values plus matching day labels.
    void setData(const std::vector<int>& data, const QStringList& labels);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    std::vector<int> m_data;
    std::vector<int> m_oldData;
    QStringList m_labels;

    QVariantAnimation *m_animation;
    qreal m_animProgress;
};

#endif // WEEKLYCHARTWIDGET_H
