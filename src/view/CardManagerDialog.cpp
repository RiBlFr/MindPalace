#include "CardManagerDialog.h"

#include <QAction>
#include <QColor>
#include <QDebug>
#include <QEvent>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPoint>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "StyledDialogs.h"

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

bool CardManagerDialog::eventFilter(QObject* watched, QEvent* event) {
    if ((watched == m_addFront || watched == m_addBack) && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            handleAddClicked();
            return true;
        }
    }

    return QDialog::eventFilter(watched, event);
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

    auto *header = new QVBoxLayout;
    header->setSpacing(2);
    auto *title = new QLabel(m_deckName, this);
    title->setObjectName("headerTitle");
    m_subtitle = new QLabel(this);
    m_subtitle->setObjectName("headerSubtitle");
    header->addWidget(title);
    header->addWidget(m_subtitle);
    root->addLayout(header);

    auto *addRow = new QHBoxLayout;
    addRow->setSpacing(8);

    m_addFront = new QLineEdit(this);
    m_addFront->setObjectName("addInput");
    m_addFront->installEventFilter(this);
    m_addFront->setPlaceholderText(tr("正面 (问题)"));

    m_addBack = new QLineEdit(this);
    m_addBack->setObjectName("addInput");
    m_addBack->installEventFilter(this);
    m_addBack->setPlaceholderText(tr("背面 (答案)"));

    auto *addBtn = new QPushButton(tr("+ 添加"), this);
    addBtn->setObjectName("addCardBtn");
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setDefault(false);
    addBtn->setAutoDefault(false);

    addRow->addWidget(m_addFront, 1);
    addRow->addWidget(m_addBack, 1);
    addRow->addWidget(addBtn, 0);
    root->addLayout(addRow);

    connect(addBtn, &QPushButton::clicked, this, &CardManagerDialog::handleAddClicked);
    // Enter in either input commits the add form.

    // Always render the table so newly added rows have a stable destination.
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
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableWidget::customContextMenuRequested,
            this, &CardManagerDialog::showTableContextMenu);

    for (const auto& card : cards) {
        appendCardRow(card, /*pendingSave=*/false);
    }
    root->addWidget(m_table, 1);

    refreshSubtitle();

    auto *footer = new QHBoxLayout;
    footer->addStretch();
    auto *closeBtn = new QPushButton(tr("关闭"), this);
    closeBtn->setObjectName("closeBtn");
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setAutoDefault(false);
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
    // Store cardId on the first column item for row-level actions.
    frontItem->setData(Qt::UserRole, card.id);
    m_table->setItem(row, 0, frontItem);
    m_table->setItem(row, 1, backItem);

    auto *wrap = new QWidget;
    auto *wrapLayout = new QHBoxLayout(wrap);
    wrapLayout->setContentsMargins(0, 0, 0, 0);
    wrapLayout->setSpacing(0);

    auto *delBtn = new QPushButton(tr("删除"), wrap);
    delBtn->setObjectName("rowDeleteBtn");
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setAutoDefault(false);
    delBtn->setFixedSize(72, 30);
    wrapLayout->addStretch(1);
    wrapLayout->addWidget(delBtn);
    wrapLayout->addStretch(1);

    if (pendingSave) {
        // Newly added rows do not have persisted ids until the dialog is reopened.
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
        StyledDialogs::info(this, tr("提示"), tr("正面和背面都不能为空。"));
        return;
    }

    emit signal_requestAddCard(m_deckName, front, back);

    // Show the row immediately; it becomes fully editable after reopening.
    appendCardRow({/*id=*/QString(), front, back}, /*pendingSave=*/true);
    refreshSubtitle();

    m_addFront->clear();
    m_addBack->clear();
    m_addFront->setFocus();
}

void CardManagerDialog::confirmAndRemoveRow(const QString& cardId,
                                            const QString& frontText,
                                            QWidget* rowMarker) {
    const bool confirmed = StyledDialogs::confirm(
        this,
        tr("删除卡片"),
        tr("确定要删除这张卡片吗？\n\n正面：%1").arg(frontText),
        /*dangerAction=*/true);
    if (!confirmed) return;

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

void CardManagerDialog::showTableContextMenu(const QPoint& pos) {
    if (!m_table) return;

    const QModelIndex idx = m_table->indexAt(pos);
    if (!idx.isValid()) return;
    const int row = idx.row();

    // Rows without persisted ids cannot be edited yet.
    auto *frontItem = m_table->item(row, 0);
    if (!frontItem) return;
    const QString cardId = frontItem->data(Qt::UserRole).toString();

    QMenu menu(this);
    auto *editAction = menu.addAction(tr("修改"));
    if (cardId.isEmpty()) {
        editAction->setEnabled(false);
        editAction->setToolTip(tr("请关闭并重新打开本对话框后再修改该卡片"));
    }

    QAction* picked = menu.exec(m_table->viewport()->mapToGlobal(pos));
    if (picked == editAction) {
        editCardAtRow(row);
    }
}

void CardManagerDialog::editCardAtRow(int row) {
    if (!m_table || row < 0 || row >= m_table->rowCount()) return;

    auto *frontItem = m_table->item(row, 0);
    auto *backItem  = m_table->item(row, 1);
    if (!frontItem || !backItem) return;

    const QString cardId = frontItem->data(Qt::UserRole).toString();
    if (cardId.isEmpty()) return;

    auto edited = StyledDialogs::getCardPair(
        this, tr("修改卡片"),
        frontItem->text(), backItem->text());
    if (!edited) return;

    const QString newFront = edited->first;
    const QString newBack  = edited->second;
    if (newFront == frontItem->text() && newBack == backItem->text()) return;

    // Update the visible row immediately; persistence is handled by the controller.
    frontItem->setText(newFront);
    backItem->setText(newBack);
    frontItem->setToolTip(newFront);
    backItem->setToolTip(newBack);

    emit signal_requestUpdateCard(m_deckName, cardId, newFront, newBack);
}
