#include "StyleUtils.h"

#include <QGraphicsDropShadowEffect>
#include <QPalette>

namespace Theme {
    extern const QColor WindowBg(0xf7f8fb);
    extern const QColor SideBg(0xf3f6fa);
    extern const QColor Surface(0xffffff);

    extern const QColor Text(0x1f2937);
    extern const QColor MutedText(0x64748b);

    extern const QColor Primary(0x2563eb);
    extern const QColor Success(0x16a34a);
}

void setBackground(QWidget *widget, const QColor &color) {
    QPalette pal = widget->palette();
    pal.setColor(QPalette::Window, color);
    widget->setAutoFillBackground(true);
    widget->setPalette(pal);
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
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 6);
    shadow->setColor(QColor(15, 23, 42, 28));
    widget->setGraphicsEffect(shadow);
}

void markSurface(QFrame *frame, bool shadow) {
    frame->setProperty("role", "surface");
    setBackground(frame, Theme::Surface);

    if (shadow) {
        addSoftShadow(frame);
    }
}

void setButtonFont(QPushButton *button, int pointSize) {
    QFont font = button->font();
    font.setPointSize(pointSize);
    font.setWeight(QFont::DemiBold);
    button->setFont(font);
}
