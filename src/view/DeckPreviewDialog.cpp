#include "DeckPreviewDialog.h"

#include "StyledDialogs.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScroller>
#include <QVariantAnimation>
#include <QVBoxLayout>

#include <cmath>
#include <functional>

namespace {

class PreviewFlipCard final : public QWidget {
public:
    PreviewFlipCard(const CardDisplayInfo& card,
                    int index,
                    bool darkMode,
                    bool editable,
                    QWidget *parent = nullptr)
            : QWidget(parent),
              m_card(card),
              m_index(index),
              m_darkMode(darkMode),
              m_editable(editable),
              m_accent(index % 2 == 0) {
        setFixedSize(220, 120);
        setCursor(Qt::PointingHandCursor);

        m_animation.setDuration(360);
        m_animation.setStartValue(0.0);
        m_animation.setEndValue(1.0);
        m_animation.setEasingCurve(QEasingCurve::InOutCubic);
        connect(&m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            m_flipProgress = value.toReal();
            update();
        });
    }

    void setSelected(bool selected) {
        if (m_selected == selected) return;
        m_selected = selected;
        update();
    }

    void setSelectCallback(std::function<void(int)> callback) {
        m_onSelect = std::move(callback);
    }

    void setEditCallback(std::function<void(int)> callback) {
        m_onEdit = std::move(callback);
    }

protected:
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton || !rect().contains(event->pos())) {
            QWidget::mouseReleaseEvent(event);
            return;
        }

        if (m_onSelect) {
            m_onSelect(m_index);
        }

        if (m_editable && editButtonRect().contains(event->pos())) {
            if (m_onEdit) m_onEdit(m_index);
            event->accept();
            return;
        }

        m_showBack = !m_showBack;
        m_animation.stop();
        m_animation.setDirection(m_showBack ? QAbstractAnimation::Forward
                                            : QAbstractAnimation::Backward);
        m_animation.start();
        event->accept();
    }

    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        const qreal compressed = std::abs(std::cos(m_flipProgress * M_PI));
        const qreal scaleX = qMax<qreal>(0.08, compressed);
        const bool drawBack = m_flipProgress >= 0.5;

        painter.translate(width() / 2.0, height() / 2.0);
        painter.scale(scaleX, 1.0);
        painter.translate(-width() / 2.0, -height() / 2.0);

        const QRectF cardRect(1.5, 1.5, width() - 3.0, height() - 3.0);
        QPainterPath path;
        path.addRoundedRect(cardRect, 10, 10);

        if (m_accent && !drawBack) {
            QLinearGradient gradient(cardRect.topLeft(), cardRect.topRight());
            gradient.setColorAt(0.0, QColor("#2346a4"));
            gradient.setColorAt(1.0, QColor("#25cbc5"));
            painter.fillPath(path, gradient);
            painter.setPen(QPen(m_darkMode ? QColor("#4da9ff") : QColor("#dce5ee"), 1.4));

            painter.save();
            painter.setClipPath(path);
            painter.setPen(QPen(QColor(255, 255, 255, m_darkMode ? 18 : 30), 1));
            for (int x = -180; x < width() + 80; x += 32) {
                painter.drawLine(QPointF(x, height()), QPointF(x + 190, 0));
            }
            painter.restore();
        } else {
            const QColor fill = drawBack
                    ? (m_darkMode ? QColor(13, 27, 45, 245) : QColor(247, 251, 255, 255))
                    : (m_darkMode ? QColor(10, 21, 35, 236) : QColor("#ffffff"));
            painter.fillPath(path, fill);
            painter.setPen(QPen(m_darkMode ? QColor(113, 135, 160, 176) : QColor("#dde3ea"), 1.2));
        }
        painter.drawPath(path);

        if (m_selected) {
            painter.setPen(QPen(QColor("#1683ff"), m_darkMode ? 2.6 : 2.2));
            painter.drawRoundedRect(cardRect.adjusted(2, 2, -2, -2), 10, 10);
        }

        const QString mainText = drawBack
                ? (m_card.back.trimmed().isEmpty() ? QStringLiteral("(无背面)") : m_card.back)
                : m_card.front;
        const QString bottomText = drawBack
                ? QStringLiteral("答案")
                : QString::number(m_index + 1);

        QColor textColor;
        QColor bottomColor;
        if (m_accent && !drawBack) {
            textColor = QColor("#ffffff");
            bottomColor = QColor("#ffffff");
        } else if (m_darkMode) {
            textColor = QColor("#f6fbff");
            bottomColor = QColor("#e8eef7");
        } else {
            textColor = QColor("#182337");
            bottomColor = QColor("#1f2937");
        }

        QFont mainFont = font();
        mainFont.setPointSize(16);
        mainFont.setWeight(QFont::Black);
        painter.setFont(mainFont);
        painter.setPen(textColor);
        painter.drawText(QRectF(16, 24, width() - 32, 48),
                         Qt::AlignCenter | Qt::TextWordWrap,
                         mainText);

        QFont bottomFont = font();
        bottomFont.setPointSize(10);
        bottomFont.setWeight(QFont::Medium);
        painter.setFont(bottomFont);
        painter.setPen(bottomColor);
        painter.drawText(QRectF(16, height() - 34, width() - 32, 22),
                         Qt::AlignCenter,
                         bottomText);

        if (m_editable && !drawBack) {
            drawEditButton(painter);
        }
    }

private:
    QRectF editButtonRect() const {
        return QRectF(width() - 43, 11, 30, 30);
    }

    void drawEditButton(QPainter& painter) {
        const QRectF bubble = editButtonRect();
        painter.setPen(QPen(m_darkMode ? QColor("#58a8ff") : QColor("#0f5ca8"), 1.4));
        painter.setBrush(m_darkMode ? QColor("#f2f7ff") : QColor("#ffffff"));
        painter.drawEllipse(bubble);

        painter.setPen(QPen(m_darkMode ? QColor("#0e315d") : QColor("#1b5f9f"), 2.2, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(bubble.left() + 10, bubble.bottom() - 10),
                         QPointF(bubble.right() - 10, bubble.top() + 10));
        painter.drawLine(QPointF(bubble.right() - 13, bubble.top() + 8),
                         QPointF(bubble.right() - 8, bubble.top() + 13));
    }

    CardDisplayInfo m_card;
    QVariantAnimation m_animation;
    std::function<void(int)> m_onSelect;
    std::function<void(int)> m_onEdit;
    int m_index = 0;
    qreal m_flipProgress = 0.0;
    bool m_darkMode = false;
    bool m_editable = false;
    bool m_accent = false;
    bool m_selected = false;
    bool m_showBack = false;
};

} // namespace

DeckPreviewDialog::DeckPreviewDialog(const QString& deckName,
                                     const std::vector<CardDisplayInfo>& cards,
                                     bool darkMode,
                                     QWidget *parent)
        : DeckPreviewDialog(deckName, cards, darkMode, Mode::StudyPreview, {}, parent) {}

DeckPreviewDialog::DeckPreviewDialog(const QString& deckName,
                                     const std::vector<CardDisplayInfo>& cards,
                                     bool darkMode,
                                     Mode mode,
                                     const ImportMetadata& metadata,
                                     QWidget *parent)
        : QDialog(parent),
          m_deckName(deckName),
          m_cards(cards),
          m_darkMode(darkMode),
          m_mode(mode),
          m_metadata(metadata) {
    setObjectName("deckPreviewDialog");
    setWindowTitle(QStringLiteral("预览牌组"));
    setModal(true);
    setFixedSize(840, 720);
    applyPreviewStyle();
    buildUi();
}

void DeckPreviewDialog::buildUi() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(30, 24, 30, 20);
    root->setSpacing(14);

    auto *titleBlock = new QVBoxLayout;
    titleBlock->setSpacing(5);
    auto *title = new QLabel(QStringLiteral("预览牌组"), this);
    title->setObjectName("previewTitle");
    m_subtitle = new QLabel(this);
    m_subtitle->setObjectName("previewSubtitle");
    titleBlock->addWidget(title);
    titleBlock->addWidget(m_subtitle);
    root->addLayout(titleBlock);

    if (m_mode == Mode::AiImportPreview) {
        auto *chips = new QHBoxLayout;
        chips->setSpacing(12);
        const QString sourceName = m_metadata.sourceFileName.isEmpty() ? QStringLiteral("待创建") : m_metadata.sourceFileName;
        const QStringList texts = {
            QStringLiteral("当前支持：Markdown"),
            QStringLiteral("来源文件：%1").arg(sourceName),
            QStringLiteral("AI摘要完成")
        };
        for (const QString& text : texts) {
            auto *chip = new QLabel(text, this);
            chip->setObjectName("previewChip");
            chip->setMinimumHeight(34);
            chip->setAlignment(Qt::AlignCenter);
            chips->addWidget(chip, 0);
        }
        chips->addStretch(1);
        root->addLayout(chips);
    }

    auto *scrollShell = new QFrame(this);
    scrollShell->setObjectName("previewScrollShell");
    auto *shellLayout = new QVBoxLayout(scrollShell);
    shellLayout->setContentsMargins(14, 14, 14, 14);

    auto *scrollArea = new QScrollArea(scrollShell);
    scrollArea->setObjectName("previewScrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    QScroller::grabGesture(scrollArea->viewport(), QScroller::TouchGesture);

    m_gridHost = new QWidget(scrollArea);
    m_gridHost->setObjectName("previewGridHost");
    m_grid = new QGridLayout(m_gridHost);
    m_grid->setContentsMargins(2, 2, 2, 2);
    m_grid->setHorizontalSpacing(26);
    m_grid->setVerticalSpacing(24);
    m_grid->setColumnStretch(0, 1);
    m_grid->setColumnStretch(1, 1);
    m_grid->setColumnStretch(2, 1);

    scrollArea->setWidget(m_gridHost);
    shellLayout->addWidget(scrollArea);
    root->addWidget(scrollShell, 1);

    auto *hint = new QLabel(m_mode == Mode::AiImportPreview
                            ? QStringLiteral("ⓘ 滚轮滑动查看更多卡片，点击卡片可翻面，右上角可编辑")
                            : QStringLiteral("ⓘ 滚轮滑动查看更多卡片，点击卡片可翻面"),
                            this);
    hint->setObjectName("previewHint");
    hint->setAlignment(Qt::AlignCenter);
    root->addWidget(hint);

    auto *footer = new QHBoxLayout;
    footer->setContentsMargins(0, 0, 0, 0);
    footer->setSpacing(12);

    auto *cancelBtn = new QPushButton(m_mode == Mode::AiImportPreview ? QStringLiteral("取消") : QStringLiteral("关闭"), this);
    cancelBtn->setObjectName("previewSecondary");
    cancelBtn->setFixedSize(176, 52);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    footer->addWidget(cancelBtn);

    if (m_mode == Mode::AiImportPreview) {
        auto *addBtn = new QPushButton(QStringLiteral("新增"), this);
        addBtn->setObjectName("previewAdd");
        addBtn->setFixedSize(176, 52);
        addBtn->setCursor(Qt::PointingHandCursor);

        m_deleteBtn = new QPushButton(QStringLiteral("删除"), this);
        m_deleteBtn->setObjectName("previewDelete");
        m_deleteBtn->setFixedSize(176, 52);
        m_deleteBtn->setCursor(Qt::PointingHandCursor);
        m_deleteBtn->setEnabled(false);

        auto *createBtn = new QPushButton(QStringLiteral("创建"), this);
        createBtn->setObjectName("previewPrimary");
        createBtn->setFixedSize(176, 52);
        createBtn->setCursor(Qt::PointingHandCursor);
        createBtn->setDefault(true);

        footer->addStretch(1);
        footer->addWidget(addBtn);
        footer->addWidget(m_deleteBtn);
        footer->addWidget(createBtn);

        connect(addBtn, &QPushButton::clicked, this, &DeckPreviewDialog::addCard);
        connect(m_deleteBtn, &QPushButton::clicked, this, &DeckPreviewDialog::deleteSelectedCard);
        connect(createBtn, &QPushButton::clicked, this, [this]() {
            if (m_cards.empty()) {
                StyledDialogs::info(this, QStringLiteral("提示"), QStringLiteral("至少需要保留一张卡片。"));
                return;
            }
            accept();
        });
    } else {
        auto *studyBtn = new QPushButton(QStringLiteral("进入学习"), this);
        studyBtn->setObjectName("previewPrimary");
        studyBtn->setFixedSize(176, 52);
        studyBtn->setCursor(Qt::PointingHandCursor);
        studyBtn->setDefault(true);
        footer->addStretch(1);
        footer->addWidget(studyBtn);
        connect(studyBtn, &QPushButton::clicked, this, &QDialog::accept);
    }

    root->addLayout(footer);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    rebuildGrid();
}

void DeckPreviewDialog::rebuildGrid() {
    if (!m_grid) return;

    // Rebuild cards after add/edit/delete so the visual index, selection ring,
    // and flip-card callbacks all stay in sync with m_cards.
    while (QLayoutItem* item = m_grid->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    m_cardWidgets.clear();

    for (int i = 0; i < static_cast<int>(m_cards.size()); ++i) {
        QWidget* card = createPreviewCard(m_cards[i], i);
        m_grid->addWidget(card, i / 3, i % 3);
        m_cardWidgets.push_back(card);
    }

    if (m_selectedIndex >= static_cast<int>(m_cards.size())) {
        m_selectedIndex = static_cast<int>(m_cards.size()) - 1;
    }
    if (m_selectedIndex < 0 && !m_cards.empty() && m_mode == Mode::AiImportPreview) {
        m_selectedIndex = 0;
    }
    setSelectedIndex(m_selectedIndex);

    if (m_subtitle) {
        const QString aiNote = m_mode == Mode::AiImportPreview
                ? QStringLiteral("（AI已总结生成，点击卡片可编辑）")
                : QStringLiteral("（点击卡片可翻面）");
        m_subtitle->setText(QStringLiteral("%1 · 共 %2 张卡片 %3")
                                    .arg(m_deckName)
                                    .arg(static_cast<int>(m_cards.size()))
                                    .arg(aiNote));
    }
}

QWidget* DeckPreviewDialog::createPreviewCard(const CardDisplayInfo& card, int index) {
    auto *previewCard = new PreviewFlipCard(card, index, m_darkMode, m_mode == Mode::AiImportPreview, this);
    previewCard->setSelectCallback([this](int selectedIndex) {
        setSelectedIndex(selectedIndex);
    });
    previewCard->setEditCallback([this](int editIndex) {
        editCard(editIndex);
    });
    previewCard->setSelected(index == m_selectedIndex);
    return previewCard;
}

void DeckPreviewDialog::addCard() {
    auto card = StyledDialogs::getCardPair(this, QStringLiteral("新增卡片"));
    if (!card) return;

    m_cards.push_back({QString(), card->first, card->second});
    m_selectedIndex = static_cast<int>(m_cards.size()) - 1;
    rebuildGrid();
}

void DeckPreviewDialog::editCard(int index) {
    if (index < 0 || index >= static_cast<int>(m_cards.size())) return;

    auto card = StyledDialogs::getCardPair(this,
                                           QStringLiteral("修改卡片"),
                                           m_cards[index].front,
                                           m_cards[index].back);
    if (!card) return;

    m_cards[index].front = card->first;
    m_cards[index].back = card->second;
    m_selectedIndex = index;
    rebuildGrid();
}

void DeckPreviewDialog::deleteSelectedCard() {
    if (m_selectedIndex < 0 || m_selectedIndex >= static_cast<int>(m_cards.size())) return;

    const QString front = m_cards[m_selectedIndex].front;
    const bool ok = StyledDialogs::confirm(this,
                                           QStringLiteral("删除卡片"),
                                           QStringLiteral("确定删除这张卡片吗？\n\n%1").arg(front),
                                           true);
    if (!ok) return;

    m_cards.erase(m_cards.begin() + m_selectedIndex);
    if (m_cards.empty()) {
        m_selectedIndex = -1;
    } else if (m_selectedIndex >= static_cast<int>(m_cards.size())) {
        m_selectedIndex = static_cast<int>(m_cards.size()) - 1;
    }
    rebuildGrid();
}

void DeckPreviewDialog::setSelectedIndex(int index) {
    m_selectedIndex = index;
    // Only one card can be the edit/delete target at a time; the blue ring is
    // deliberately separate from the flip state so selecting a card is cheap.
    for (int i = 0; i < static_cast<int>(m_cardWidgets.size()); ++i) {
        if (auto *card = dynamic_cast<PreviewFlipCard*>(m_cardWidgets[i])) {
            card->setSelected(i == m_selectedIndex);
        }
    }
    if (m_deleteBtn) {
        m_deleteBtn->setEnabled(m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_cards.size()));
    }
}

void DeckPreviewDialog::applyPreviewStyle() {
    if (m_darkMode) {
        setStyleSheet(QStringLiteral(R"(
            QDialog#deckPreviewDialog {
                background-color: rgba(6, 16, 29, 246);
                border: 1px solid #27b8f4;
                border-radius: 14px;
            }
            QLabel#previewTitle {
                color: #f4fbff;
                font-size: 27px;
                font-weight: 900;
            }
            QLabel#previewSubtitle, QLabel#previewHint {
                color: #9fb3ca;
                font-size: 15px;
                font-weight: 600;
            }
            QLabel#previewChip {
                color: #f2f8ff;
                background-color: rgba(7, 17, 29, 190);
                border: 1px solid rgba(104, 133, 166, 170);
                border-radius: 6px;
                padding: 0 14px;
                font-size: 14px;
                font-weight: 600;
            }
            QFrame#previewScrollShell {
                background-color: rgba(7, 17, 29, 206);
                border: 1px solid rgba(104, 133, 166, 155);
                border-radius: 18px;
            }
            QScrollArea#previewScrollArea,
            QWidget#previewGridHost {
                background: transparent;
                border: none;
            }
            QScrollBar:vertical {
                background: rgba(255,255,255,28);
                width: 11px;
                margin: 0;
                border-radius: 5px;
            }
            QScrollBar::handle:vertical {
                background: #28aafa;
                min-height: 86px;
                border-radius: 5px;
            }
            QScrollBar::add-line:vertical,
            QScrollBar::sub-line:vertical {
                height: 0;
                border: none;
                background: transparent;
            }
            QPushButton#previewSecondary,
            QPushButton#previewPrimary,
            QPushButton#previewAdd,
            QPushButton#previewDelete {
                border-radius: 9px;
                font-size: 17px;
                font-weight: 900;
            }
            QPushButton#previewSecondary {
                color: #f8fbff;
                background-color: rgba(8, 18, 30, 210);
                border: 1px solid rgba(166, 185, 209, 190);
            }
            QPushButton#previewAdd {
                color: #78f269;
                background-color: rgba(8, 18, 30, 210);
                border: 1px solid #78f269;
            }
            QPushButton#previewDelete {
                color: #ff5a5f;
                background-color: rgba(8, 18, 30, 210);
                border: 1px solid #ff5a5f;
            }
            QPushButton#previewDelete:disabled {
                color: rgba(255, 90, 95, 95);
                border-color: rgba(255, 90, 95, 95);
            }
            QPushButton#previewPrimary {
                color: #ffffff;
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                                            stop:0 #0c4f9d, stop:1 #0f75cf);
                border: 1px solid #2c9cff;
            }
        )"));
    } else {
        setStyleSheet(QStringLiteral(R"(
            QDialog#deckPreviewDialog {
                background-color: rgba(255, 255, 255, 248);
                border: 1px solid #e3e8ef;
                border-radius: 14px;
            }
            QLabel#previewTitle {
                color: #1b2b43;
                font-size: 27px;
                font-weight: 900;
            }
            QLabel#previewSubtitle, QLabel#previewHint {
                color: #65758c;
                font-size: 15px;
                font-weight: 600;
            }
            QLabel#previewChip {
                color: #1b2b43;
                background-color: #f8fafc;
                border: 1px solid #d7dde5;
                border-radius: 6px;
                padding: 0 14px;
                font-size: 14px;
                font-weight: 600;
            }
            QFrame#previewScrollShell {
                background-color: #ffffff;
                border: 1px solid #d9e0e8;
                border-radius: 18px;
            }
            QScrollArea#previewScrollArea,
            QWidget#previewGridHost {
                background: transparent;
                border: none;
            }
            QScrollBar:vertical {
                background: transparent;
                width: 11px;
                margin: 0;
                border-radius: 5px;
            }
            QScrollBar::handle:vertical {
                background: #888888;
                min-height: 86px;
                border-radius: 5px;
            }
            QScrollBar::add-line:vertical,
            QScrollBar::sub-line:vertical {
                height: 0;
                border: none;
                background: transparent;
            }
            QPushButton#previewSecondary,
            QPushButton#previewPrimary,
            QPushButton#previewAdd,
            QPushButton#previewDelete {
                border-radius: 9px;
                font-size: 17px;
                font-weight: 900;
            }
            QPushButton#previewSecondary {
                color: #111827;
                background-color: #ffffff;
                border: 1px solid #d7dde5;
            }
            QPushButton#previewAdd {
                color: #16a34a;
                background-color: #ffffff;
                border: 1px solid #22c55e;
            }
            QPushButton#previewDelete {
                color: #ef4444;
                background-color: #ffffff;
                border: 1px solid #ef4444;
            }
            QPushButton#previewDelete:disabled {
                color: #fca5a5;
                border-color: #fca5a5;
            }
            QPushButton#previewPrimary {
                color: #ffffff;
                background-color: #5688b8;
                border: 1px solid #5688b8;
            }
        )"));
    }
}
