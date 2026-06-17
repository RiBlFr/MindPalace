#include "WeeklyChartWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QEasingCurve>
#include <algorithm>

WeeklyChartWidget::WeeklyChartWidget(QWidget *parent) : QWidget(parent), m_animProgress(1.0) {
    setMinimumHeight(150);

    m_data = {0, 0, 0, 0, 0, 0, 0};
    m_oldData = {0, 0, 0, 0, 0, 0, 0};
    m_labels = {"-", "-", "-", "-", "-", "-", "今"};

    m_animation = new QVariantAnimation(this);
    m_animation->setDuration(450);
    m_animation->setEasingCurve(QEasingCurve::OutBack);

    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value){
        m_animProgress = value.toReal();
        update();
    });
}

void WeeklyChartWidget::setData(const std::vector<int>& data, const QStringList& labels) {
    if (data.size() == 7 && labels.size() == 7) {
        m_oldData = m_data;
        m_data = data;
        m_labels = labels;

        // Restart growth animation whenever the data changes.
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
    int paddingTop = 25;    // 给顶部留出写数字的空间
    int paddingBottom = 25; // 给底部留出写日期的空间

    int drawWidth = width() - paddingLeftRight * 2;
    int drawHeight = height() - paddingTop - paddingBottom;

    int maxVal = 0;
    for (int v : m_data) { if (v > maxVal) maxVal = v; }
    for (int v : m_oldData) { if (v > maxVal) maxVal = v; }
    if (maxVal == 0) maxVal = 10;

    int numBars = 7;
    float step = drawWidth / (float)numBars;
    float barWidth = step * 0.55f;

    QColor textColor = palette().color(QPalette::WindowText);

    for (int i = 0; i < numBars; ++i) {
        int oldVal = m_oldData[i];
        int targetVal = m_data[i];

        int oldBarHeight = (int)(drawHeight * ((float)oldVal / maxVal));
        int targetBarHeight = (int)(drawHeight * ((float)targetVal / maxVal));

        if (oldBarHeight < 2) oldBarHeight = 2;
        if (targetBarHeight < 2) targetBarHeight = 2;

        int barHeight = oldBarHeight + (targetBarHeight - oldBarHeight) * m_animProgress;

        // 顶部的数字也同样应用增量插值，实现数字的平滑滚动
        int currentDisplayVal = oldVal + (targetVal - oldVal) * m_animProgress;

        int cx = paddingLeftRight + i * step + (step - barWidth) / 2;
        int cy = paddingTop + drawHeight - barHeight;

        QRectF barRect(cx, cy, barWidth, barHeight);
        QPainterPath path;
        path.addRoundedRect(barRect, barWidth / 2.0, barWidth / 2.0);

        QLinearGradient barGradient(barRect.topLeft(), barRect.bottomLeft());
        barGradient.setColorAt(0.0, QColor("#8d63ff"));
        barGradient.setColorAt(1.0, QColor("#2a95ff"));

        painter.setPen(Qt::NoPen);
        painter.setBrush(barGradient);
        painter.drawPath(path);

        painter.setPen(textColor);
        QFont font = painter.font();
        font.setPointSize(9);
        font.setBold(true);
        painter.setFont(font);

        // 绘制跟随柱子顶部的数字
        QRectF numRect(cx - 10, cy - 18, barWidth + 20, 15);
        painter.drawText(numRect, Qt::AlignCenter, QString::number(currentDisplayVal));

        font.setBold(false);
        painter.setFont(font);
        QRectF labelRect(cx - 10, paddingTop + drawHeight + 5, barWidth + 20, 15);
        painter.drawText(labelRect, Qt::AlignCenter, m_labels[i]);
    }
}
