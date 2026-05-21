#ifndef MINDPALACE_STORAGEMANAGER_H
#define MINDPALACE_STORAGEMANAGER_H

#include "model/Deck.h"
#include <QString>

namespace MindPalace::Service {

    class StorageManager {
    public:
        StorageManager() = delete;

        static bool saveDeck(const Model::Deck& deck, const QString& filePath);
        static bool loadDeck(const QString& filePath, Model::Deck& deck);
    };

} // namespace MindPalace::Service

#endif // MINDPALACE_STORAGEMANAGER_H
