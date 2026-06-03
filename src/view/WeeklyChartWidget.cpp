#include "WeeklyChartWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

WeeklyChartWidget::WeeklyChartWidget(QWidget *parent) : QWidget(parent) {
    // 设置一个最小高度，保证图表有足够的空间展示
    setMinimumHeight(150);
    
    // 初始化默认空数据（7天）
    m_data = {0, 0, 0, 0, 0, 0, 0};
    m_labels = {"-", "-", "-", "-", "-", "-", "今"};
}

void WeeklyChartWidget::setData(const std::vector<int>& data, const QStringList& labels) {
    if (data.size() == 7 && labels.size() == 7) {
        m_data = data;
        m_labels = labels;
        update(); // 触发重绘
    }
}

void WeeklyChartWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing); // 开启抗锯齿，圆角更平滑

    // 1. 计算图表尺寸与边距
    int paddingLeftRight = 10;
    int paddingTop = 25;    // 给顶部留出写数字的空间
    int paddingBottom = 25; // 给底部留出写日期的空间
    
    int drawWidth = width() - paddingLeftRight * 2;
    int drawHeight = height() - paddingTop - paddingBottom;

    // 2. 寻找最大值，用于计算柱子的高度比例
    int maxVal = 0;
    for (int v : m_data) {
        if (v > maxVal) maxVal = v;
    }
    if (maxVal == 0) maxVal = 10; // 避免除以 0，当没有数据时默认比例

    // 3. 计算单根柱子的宽度和间距
    int numBars = 7;
    float step = drawWidth / (float)numBars;
    float barWidth = step * 0.55f; // 柱子占据每份空间的 55%
    
    // 准备画笔和画刷
    QColor barColor("#4A90E2"); // 使用与你 UI 呼应的现代蓝
    QColor textColor("#666666"); // 柔和的文字颜色

    // 4. 开始遍历绘制 7 根柱子
    for (int i = 0; i < numBars; ++i) {
        int val = m_data[i];
        
        // 计算当前柱子的高度 (映射到实际像素)
        float heightRatio = (float)val / maxVal;
        int barHeight = (int)(drawHeight * heightRatio);
        
        // 如果值为 0，也给个2像素的保底高度，视觉上更好看
        if (barHeight < 2) barHeight = 2; 

        // 计算柱子的 X, Y 坐标
        int cx = paddingLeftRight + i * step + (step - barWidth) / 2;
        int cy = paddingTop + drawHeight - barHeight;

        // 绘制带圆角的柱子 (只让顶部有圆角会更好看，这里用一个巧妙的画法)
        QRectF barRect(cx, cy, barWidth, barHeight);
        QPainterPath path;
        path.addRoundedRect(barRect, barWidth / 2.0, barWidth / 2.0);
        
        painter.setPen(Qt::NoPen);
        painter.setBrush(barColor);
        painter.drawPath(path);

        // 绘制柱子顶部的数字
        painter.setPen(textColor);
        QFont font = painter.font();
        font.setPointSize(9);
        font.setBold(true);
        painter.setFont(font);
        
        QRectF numRect(cx - 10, cy - 18, barWidth + 20, 15);
        painter.drawText(numRect, Qt::AlignCenter, QString::number(val));

        // 绘制柱子底部的日期标签
        font.setBold(false);
        painter.setFont(font);
        QRectF labelRect(cx - 10, paddingTop + drawHeight + 5, barWidth + 20, 15);
        painter.drawText(labelRect, Qt::AlignCenter, m_labels[i]);
    }
}