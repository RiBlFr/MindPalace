#ifndef STYLEUTILS_H
#define STYLEUTILS_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QFrame>

namespace Theme {
    extern const QColor WindowBg;
    extern const QColor SideBg;
    extern const QColor Surface;

    extern const QColor Text;
    extern const QColor MutedText;

    extern const QColor Primary;
    extern const QColor Success;
}

void setBackground(QWidget *widget, const QColor &color);
void setTextColor(QWidget *widget, const QColor &color);
void setLabelStyle(QLabel *label, int pointSize, QFont::Weight weight = QFont::Normal, const QColor &color = Theme::Text);
void addSoftShadow(QWidget *widget);
void markSurface(QFrame *frame, bool shadow = true);
void setButtonFont(QPushButton *button, int pointSize = 11);

#endif // STYLEUTILS_H

