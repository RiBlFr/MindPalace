#ifndef DECKPREVIEWDIALOG_H
#define DECKPREVIEWDIALOG_H

#include "CardManagerDialog.h"

#include <QDialog>
#include <vector>

class DeckPreviewDialog final : public QDialog {
    Q_OBJECT

public:
    enum class Mode {
        StudyPreview,
        AiImportPreview
    };

    struct ImportMetadata {
        QString sourceFileName;
        QString summary;
    };

    DeckPreviewDialog(const QString& deckName,
                      const std::vector<CardDisplayInfo>& cards,
                      bool darkMode,
                      QWidget *parent = nullptr);
    DeckPreviewDialog(const QString& deckName,
                      const std::vector<CardDisplayInfo>& cards,
                      bool darkMode,
                      Mode mode,
                      const ImportMetadata& metadata,
                      QWidget *parent = nullptr);

    QString deckName() const { return m_deckName; }
    std::vector<CardDisplayInfo> cards() const { return m_cards; }

private:
    QWidget* createPreviewCard(const CardDisplayInfo& card, int index);
    void buildUi();
    void rebuildGrid();
    void applyPreviewStyle();
    void addCard();
    void editCard(int index);
    void deleteSelectedCard();
    void setSelectedIndex(int index);

    QString m_deckName;
    std::vector<CardDisplayInfo> m_cards;
    bool m_darkMode = false;
    Mode m_mode = Mode::StudyPreview;
    ImportMetadata m_metadata;
    int m_selectedIndex = -1;
    class QGridLayout* m_grid = nullptr;
    class QWidget* m_gridHost = nullptr;
    class QLabel* m_subtitle = nullptr;
    class QPushButton* m_deleteBtn = nullptr;
    std::vector<QWidget*> m_cardWidgets;
};

#endif // DECKPREVIEWDIALOG_H
