#include "StyleUtils.h"

#include <QGraphicsDropShadowEffect>
#include <QPalette>

namespace Theme {
    extern const QColor WindowBg(0xf7f8fb);

    // 注入 Alpha 通道（透明度）。
    // 将原本实心的背景改为半透明。255是完全不透明，我们使用 150 和 180 营造通透感。
    extern const QColor SideBg(243, 246, 250, 150);
    extern const QColor Surface(255, 255, 255, 180);

    extern const QColor Text(0x1f2937);
    extern const QColor MutedText(0x64748b);

    extern const QColor Primary(0x8B75FA);
    extern const QColor Success(0x16a34a);
}

void setBackground(QWidget *widget, const QColor &color) {
    if (color.alpha() < 255) {
        // 【关键修复】动态生成唯一 ID，确保样式绝对隔离，保护按钮原色！
        if (widget->objectName().isEmpty()) {
            widget->setObjectName(QString("transBg_%1").arg(reinterpret_cast<quintptr>(widget)));
        }
        widget->setStyleSheet(QString("#%1 { background-color: rgba(%2, %3, %4, %5); }")
                              .arg(widget->objectName())
                              .arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha()));
    } else {
        QPalette pal = widget->palette();
        pal.setColor(QPalette::Window, color);
        widget->setAutoFillBackground(true);
        widget->setPalette(pal);
    }
}

void setGradientBackground(QWidget *widget, bool isAurora) {
    if (widget->objectName().isEmpty()) {
        widget->setObjectName("gradientBgWidget");
    }

    if (isAurora) {
        // 极光模式：紫蓝渐变
        QString newStyle = QString("#%1 { background: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1, stop:0 #e0c3fc, stop:1 #8ec5fc); }").arg(widget->objectName());
        widget->setStyleSheet(widget->styleSheet() + "\n" + newStyle);
    } else {
        // 经典模式：纯白/浅灰底色
        QString newStyle = QString("#%1 { background: #f7f8fb; }").arg(widget->objectName());
        widget->setStyleSheet(widget->styleSheet() + "\n" + newStyle);
    }
}

void setTextColor(QWidget *widget, const QColor &color) {
    QPalette pal = widget->palette();
    pal.setColor(QPalette::WindowText, color);
    pal.setColor(QPalette::ButtonText, color);
    widget->setPalette(pal);
}

void setLabelStyle(QLabel *label, int pointSize, QFont::Weight weight, const QColor &color) {
    QFont font = label->font();
    font.setPointSize(pointSize);
    font.setWeight(weight);
    label->setFont(font);
    setTextColor(label, color);
}

void addSoftShadow(QWidget *widget) {
    auto *shadow = new QGraphicsDropShadowEffect(widget);
    // 1. 将 BlurRadius 提升到惊人的 60，让阴影边缘彻底虚化
    shadow->setBlurRadius(60);
    // 2. 将垂直偏移量加大到 20，营造强烈的悬浮高度感
    shadow->setOffset(0, 20);
    // 3. 极其关键：不要用纯黑！使用一种偏紫蓝色的极低透明度颜色，呼应你的渐变背景
    shadow->setColor(QColor(60, 70, 110, 28));
    widget->setGraphicsEffect(shadow);
}

void markSurface(QFrame *frame, bool shadow) {
    Q_UNUSED(shadow);
    frame->setProperty("role", "surface");
}

void setButtonFont(QPushButton *button, int pointSize) {
    QFont font = button->font();
    font.setPointSize(pointSize);
    font.setWeight(QFont::DemiBold);
    button->setFont(font);
}