#ifndef MINDPALACE_CARDMANAGERDIALOG_H
#define MINDPALACE_CARDMANAGERDIALOG_H

#include <QDialog>
#include <QString>
#include <vector>

class QLabel;
class QLineEdit;
class QTableWidget;

struct CardDisplayInfo {
    QString id;
    QString front;
    QString back;
};

/**
 * @brief 卡片管理对话框：展示某个牌组的所有卡片，并支持单张删除。
 *
 * 本对话框只负责 UI 呈现与用户交互；真正的删除业务（DeckController 落盘）
 * 通过 signal_requestDeleteCard 转交给上层调度。
 */
class CardManagerDialog : public QDialog {
    Q_OBJECT

public:
    CardManagerDialog(const QString& deckName,
                      const std::vector<CardDisplayInfo>& cards,
                      QWidget* parent = nullptr);

signals:
    void signal_requestDeleteCard(const QString& deckName, const QString& cardId);
    void signal_requestAddCard(const QString& deckName, const QString& front, const QString& back);
    void signal_requestUpdateCard(const QString& deckName, const QString& cardId,
                                  const QString& newFront, const QString& newBack);

private:
    void buildUi(const std::vector<CardDisplayInfo>& cards);
    void applyStyleSheet();
    void appendCardRow(const CardDisplayInfo& card, bool pendingSave);
    void handleAddClicked();
    void confirmAndRemoveRow(const QString& cardId, const QString& frontText, QWidget* rowMarker);
    void refreshSubtitle();
    void showTableContextMenu(const QPoint& pos);
    void editCardAtRow(int row);

    QString       m_deckName;
    QLabel*       m_subtitle = nullptr;
    QTableWidget* m_table    = nullptr;
    QLineEdit*    m_addFront = nullptr;
    QLineEdit*    m_addBack  = nullptr;
};

#endif // MINDPALACE_CARDMANAGERDIALOG_H
