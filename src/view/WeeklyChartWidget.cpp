#include "WeeklyChartWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QEasingCurve> // 缓动曲线引擎

WeeklyChartWidget::WeeklyChartWidget(QWidget *parent) : QWidget(parent), m_animProgress(1.0) {
    setMinimumHeight(150);
    m_data = {0, 0, 0, 0, 0, 0, 0};
    m_labels = {"-", "-", "-", "-", "-", "-", "今"};

    // 【核心注入】初始化弹性生长动画
    m_animation = new QVariantAnimation(this);
    m_animation->setDuration(850); // 850毫秒的平滑生长
    m_animation->setEasingCurve(QEasingCurve::OutBounce);

    // 当动画数值改变时，更新进度并触发界面重绘
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value){
        m_animProgress = value.toReal();
        update();
    });
}

void WeeklyChartWidget::setData(const std::vector<int>& data, const QStringList& labels) {
    if (data.size() == 7 && labels.size() == 7) {
        m_data = data;
        m_labels = labels;

        // 【触发】每次注入新数据时，重启生长动画
        m_animation->stop();
        m_animation->setStartValue(0.0);
        m_animation->setEndValue(1.0);
        m_animation->start();
    }
}

void WeeklyChartWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int paddingLeftRight = 10;
    int paddingTop = 25;
    int paddingBottom = 25;

    int drawWidth = width() - paddingLeftRight * 2;
    int drawHeight = height() - paddingTop - paddingBottom;

    int maxVal = 0;
    for (int v : m_data) {
        if (v > maxVal) maxVal = v;
    }
    if (maxVal == 0) maxVal = 10;

    int numBars = 7;
    float step = drawWidth / (float)numBars;
    float barWidth = step * 0.55f;

    QColor barColor("#8B75FA");
    QColor textColor("#64748b");

    for (int i = 0; i < numBars; ++i) {
        int val = m_data[i];

        float heightRatio = (float)val / maxVal;
        int targetBarHeight = (int)(drawHeight * heightRatio);
        if (targetBarHeight < 2) targetBarHeight = 2;

        // 【微动效核心】当前柱子的高度 = 最终目标高度 * 动画进度
        int barHeight = (int)(targetBarHeight * m_animProgress);

        int cx = paddingLeftRight + i * step + (step - barWidth) / 2;
        int cy = paddingTop + drawHeight - barHeight;

        QRectF barRect(cx, cy, barWidth, barHeight);
        QPainterPath path;
        path.addRoundedRect(barRect, barWidth / 2.0, barWidth / 2.0);

        painter.setPen(Qt::NoPen);
        painter.setBrush(barColor);
        painter.drawPath(path);

        painter.setPen(textColor);
        QFont font = painter.font();
        font.setPointSize(9);
        font.setBold(true);
        painter.setFont(font);

        // 让顶部的数字也跟随着柱子一起“长”上去
        QRectF numRect(cx - 10, cy - 18, barWidth + 20, 15);
        painter.drawText(numRect, Qt::AlignCenter, QString::number(val));

        font.setBold(false);
        painter.setFont(font);
        QRectF labelRect(cx - 10, paddingTop + drawHeight + 5, barWidth + 20, 15);
        painter.drawText(labelRect, Qt::AlignCenter, m_labels[i]);
    }
}