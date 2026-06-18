#include "StyleUtils.h"

#include <QGraphicsDropShadowEffect>
#include <QPalette>

namespace Theme {
    extern const QColor WindowBg(0xf7f8fb);

    // Shared colors for custom painting and fallback inline styles.
    extern const QColor SideBg(243, 246, 250, 150);
    extern const QColor Surface(13, 22, 34, 218);

    extern const QColor Text(0x1f2937);
    extern const QColor MutedText(0x64748b);

    extern const QColor Primary(0x8B75FA);
    extern const QColor Success(0x16a34a);
}

void setBackground(QWidget *widget, const QColor &color) {
    if (color.alpha() < 255) {
        // Scope the inline background style to this widget only.
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
    shadow->setBlurRadius(36);
    shadow->setOffset(0, 14);
    shadow->setColor(QColor(0, 0, 0, 120));
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
