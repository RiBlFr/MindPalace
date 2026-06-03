#ifndef WEEKLYCHARTWIDGET_H
#define WEEKLYCHARTWIDGET_H

#include <QWidget>
#include <vector>
#include <QStringList>
#include <QVariantAnimation> // 引入动画引擎

class WeeklyChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit WeeklyChartWidget(QWidget *parent = nullptr);

    // 接收后端传来的 7 天数据和对应的日期标签（如 "周一", "周二"）
    void setData(const std::vector<int>& data, const QStringList& labels);

protected:
    // 核心绘制逻辑
    void paintEvent(QPaintEvent *event) override;

private:
    std::vector<int> m_data;
    QStringList m_labels;

    // 动画引擎与进度变量
    QVariantAnimation *m_animation;
    qreal m_animProgress;
};

#endif // WEEKLYCHARTWIDGET_H