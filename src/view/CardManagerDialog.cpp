#include "CardManagerDialog.h"

#include <QColor>
#include <QDebug>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

CardManagerDialog::CardManagerDialog(const QString& deckName,
                                     const std::vector<CardDisplayInfo>& cards,
                                     QWidget* parent)
    : QDialog(parent), m_deckName(deckName) {
    setObjectName("cardManagerDialog");
    setWindowTitle(tr("管理卡片"));
    setMinimumSize(760, 400);
    applyStyleSheet();
    buildUi(cards);
}

void CardManagerDialog::applyStyleSheet() {
    QFile qssFile(":/styles/CardManagerDialog.qss");
    if (!qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "CardManagerDialog: failed to load QSS:" << qssFile.errorString();
        return;
    }
    setStyleSheet(QString::fromUtf8(qssFile.readAll()));
}

void CardManagerDialog::buildUi(const std::vector<CardDisplayInfo>& cards) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 18);
    root->setSpacing(14);

    // ---- Header ----
    auto *header = new QVBoxLayout;
    header->setSpacing(2);
    auto *title = new QLabel(m_deckName, this);
    title->setObjectName("headerTitle");
    m_subtitle = new QLabel(this);
    m_subtitle->setObjectName("headerSubtitle");
    header->addWidget(title);
    header->addWidget(m_subtitle);
    root->addLayout(header);

    // ---- Inline add form (always visible at the top) ----
    auto *addRow = new QHBoxLayout;
    addRow->setSpacing(8);

    m_addFront = new QLineEdit(this);
    m_addFront->setObjectName("addInput");
    m_addFront->setPlaceholderText(tr("正面 (问题)"));

    m_addBack = new QLineEdit(this);
    m_addBack->setObjectName("addInput");
    m_addBack->setPlaceholderText(tr("背面 (答案)"));

    auto *addBtn = new QPushButton(tr("+ 添加"), this);
    addBtn->setObjectName("addCardBtn");
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setDefault(true);

    addRow->addWidget(m_addFront, 1);
    addRow->addWidget(m_addBack, 1);
    addRow->addWidget(addBtn, 0);
    root->addLayout(addRow);

    connect(addBtn, &QPushButton::clicked, this, &CardManagerDialog::handleAddClicked);
    // Enter 键在任一输入框中也触发添加
    connect(m_addFront, &QLineEdit::returnPressed, this, &CardManagerDialog::handleAddClicked);
    connect(m_addBack,  &QLineEdit::returnPressed, this, &CardManagerDialog::handleAddClicked);

    // ---- Table (always rendered, even when empty, so add 后能立即看到行) ----
    m_table = new QTableWidget(0, 3, this);
    m_table->setObjectName("cardTable");
    m_table->setHorizontalHeaderLabels({tr("正面"), tr("背面"), tr("操作")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->setColumnWidth(2, 110);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(70);
    m_table->setAlternatingRowColors(true);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setFrameShape(QFrame::NoFrame);

    for (const auto& card : cards) {
        appendCardRow(card, /*pendingSave=*/false);
    }
    root->addWidget(m_table, 1);

    refreshSubtitle();

    // ---- Footer ----
    auto *footer = new QHBoxLayout;
    footer->addStretch();
    auto *closeBtn = new QPushButton(tr("关闭"), this);
    closeBtn->setObjectName("closeBtn");
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    footer->addWidget(closeBtn);
    root->addLayout(footer);
}

void CardManagerDialog::appendCardRow(const CardDisplayInfo& card, bool pendingSave) {
    const int row = m_table->rowCount();
    m_table->insertRow(row);

    auto *frontItem = new QTableWidgetItem(card.front);
    auto *backItem  = new QTableWidgetItem(card.back);
    frontItem->setToolTip(card.front);
    backItem->setToolTip(card.back);
    m_table->setItem(row, 0, frontItem);
    m_table->setItem(row, 1, backItem);

    auto *wrap = new QWidget;
    auto *wrapLayout = new QHBoxLayout(wrap);
    wrapLayout->setContentsMargins(0, 0, 0, 0);
    wrapLayout->setSpacing(0);

    auto *delBtn = new QPushButton(tr("删除"), wrap);
    delBtn->setObjectName("rowDeleteBtn");
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setFixedSize(72, 30);
    wrapLayout->addStretch(1);
    wrapLayout->addWidget(delBtn);
    wrapLayout->addStretch(1);

    if (pendingSave) {
        // 本次会话内新加的卡，ID 尚未从后端回传；先禁用删除按钮，待用户重新打开该对话框
        delBtn->setEnabled(false);
        delBtn->setToolTip(tr("关闭并重新打开本对话框后即可删除该卡片"));
        frontItem->setForeground(QColor("#9ca3af"));
        backItem->setForeground(QColor("#9ca3af"));
    } else {
        const QString cardId    = card.id;
        const QString frontText = card.front;
        connect(delBtn, &QPushButton::clicked, this, [this, cardId, frontText, wrap]() {
            confirmAndRemoveRow(cardId, frontText, wrap);
        });
    }

    m_table->setCellWidget(row, 2, wrap);
}

void CardManagerDialog::handleAddClicked() {
    const QString front = m_addFront->text().trimmed();
    const QString back  = m_addBack->text().trimmed();
    if (front.isEmpty() || back.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("正面和背面都不能为空。"));
        return;
    }

    emit signal_requestAddCard(m_deckName, front, back);

    // 前端即时反馈：立即在表格末尾追加一行（pendingSave 标记，禁用删除按钮）
    appendCardRow({/*id=*/QString(), front, back}, /*pendingSave=*/true);
    refreshSubtitle();

    m_addFront->clear();
    m_addBack->clear();
    m_addFront->setFocus();
}

void CardManagerDialog::confirmAndRemoveRow(const QString& cardId,
                                            const QString& frontText,
                                            QWidget* rowMarker) {
    const auto answer = QMessageBox::question(
        this,
        tr("删除卡片"),
        tr("确定要删除这张卡片吗？\n\n正面：%1").arg(frontText),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) return;

    if (m_table) {
        for (int r = 0; r < m_table->rowCount(); ++r) {
            if (m_table->cellWidget(r, 2) == rowMarker) {
                m_table->removeRow(r);
                break;
            }
        }
        refreshSubtitle();
    }

    emit signal_requestDeleteCard(m_deckName, cardId);
}

void CardManagerDialog::refreshSubtitle() {
    if (m_subtitle && m_table) {
        m_subtitle->setText(tr("共 %1 张卡片").arg(m_table->rowCount()));
    }
}
