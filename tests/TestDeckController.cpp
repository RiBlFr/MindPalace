#include <QTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "controller/deckcontroller.h"
#include "model/Deck.h"

using MindPalace::Controller::DeckController;

class TestDeckController : public QObject {
    Q_OBJECT

private slots:
    void createDeck_success();
    void createDeck_emptyName_returnsFalse();
    void createDeck_duplicateName_returnsFalse();
    void createDeck_persistsFileToDisk();

    void getDecks_emptyOnStart();
    void getDecks_returnsAllCreated();

    void deleteDeck_success();
    void deleteDeck_removesFileFromDisk();
    void deleteDeck_nonexistent_returnsFalse();

    void renameDeck_success();
    void renameDeck_emptyNewName_returnsFalse();
    void renameDeck_duplicateNewName_returnsFalse();
    void renameDeck_nonexistentDeck_returnsFalse();
    void renameDeck_persistsNewNameToDisk();

    void persistence_deckSurvivesReload();
};

// -----------------------------------------------------------------------

void TestDeckController::createDeck_success() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    DeckController ctrl(tmp.path());
    QVERIFY(ctrl.createDeck("Alpha"));
    QCOMPARE(ctrl.getDecks().size(), 1u);
    QCOMPARE(ctrl.getDecks()[0].deckName, "Alpha");
}

void TestDeckController::createDeck_emptyName_returnsFalse() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    DeckController ctrl(tmp.path());
    QVERIFY(!ctrl.createDeck(""));
    QCOMPARE(ctrl.getDecks().size(), 0u);
}

void TestDeckController::createDeck_duplicateName_returnsFalse() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    DeckController ctrl(tmp.path());
    QVERIFY(ctrl.createDeck("Beta"));
    QVERIFY(!ctrl.createDeck("Beta"));
    QCOMPARE(ctrl.getDecks().size(), 1u);
}

void TestDeckController::createDeck_persistsFileToDisk() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    DeckController ctrl(tmp.path());
    QVERIFY(ctrl.createDeck("Gamma"));

    const QString deckId = ctrl.getDecks()[0].deckId;
    const QString filePath = QDir(tmp.path()).filePath(deckId + ".json");
    QVERIFY(QFile::exists(filePath));
}

// -----------------------------------------------------------------------

void TestDeckController::getDecks_emptyOnStart() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    DeckController ctrl(tmp.path());
    QCOMPARE(ctrl.getDecks().size(), 0u);
}

void TestDeckController::getDecks_returnsAllCreated() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    DeckController ctrl(tmp.path());
    QVERIFY(ctrl.createDeck("One"));
    QVERIFY(ctrl.createDeck("Two"));
    QVERIFY(ctrl.createDeck("Three"));
    QCOMPARE(ctrl.getDecks().size(), 3u);
}

// -----------------------------------------------------------------------

void TestDeckController::deleteDeck_success() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    DeckController ctrl(tmp.path());
    QVERIFY(ctrl.createDeck("ToDelete"));
    QVERIFY(ctrl.deleteDeck("ToDelete"));
    QCOMPARE(ctrl.getDecks().size(), 0u);
}

void TestDeckController::deleteDeck_removesFileFromDisk() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    DeckController ctrl(tmp.path());
    QVERIFY(ctrl.createDeck("ToDelete"));

    const QString deckId = ctrl.getDecks()[0].deckId;
    const QString filePath = QDir(tmp.path()).filePath(deckId + ".json");

    QVERIFY(ctrl.deleteDeck("ToDelete"));
    QVERIFY(!QFile::exists(filePath));
}

void TestDeckController::deleteDeck_nonexistent_returnsFalse() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    DeckController ctrl(tmp.path());
    QVERIFY(!ctrl.deleteDeck("Ghost"));
}

// -----------------------------------------------------------------------

void TestDeckController::renameDeck_success() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    DeckController ctrl(tmp.path());
    QVERIFY(ctrl.createDeck("Old"));
    QVERIFY(ctrl.renameDeck("Old", "New"));

    QCOMPARE(ctrl.getDecks().size(), 1u);
    QCOMPARE(ctrl.getDecks()[0].deckName, "New");
}

void TestDeckController::renameDeck_emptyNewName_returnsFalse() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    DeckController ctrl(tmp.path());
    QVERIFY(ctrl.createDeck("Stable"));
    QVERIFY(!ctrl.renameDeck("Stable", ""));
    QCOMPARE(ctrl.getDecks()[0].deckName, "Stable");
}

void TestDeckController::renameDeck_duplicateNewName_returnsFalse() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    DeckController ctrl(tmp.path());
    QVERIFY(ctrl.createDeck("First"));
    QVERIFY(ctrl.createDeck("Second"));
    QVERIFY(!ctrl.renameDeck("First", "Second"));
    QCOMPARE(ctrl.getDecks().size(), 2u);
}

void TestDeckController::renameDeck_nonexistentDeck_returnsFalse() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    DeckController ctrl(tmp.path());
    QVERIFY(!ctrl.renameDeck("NoSuchDeck", "Whatever"));
}

void TestDeckController::renameDeck_persistsNewNameToDisk() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    {
        DeckController ctrl(tmp.path());
        QVERIFY(ctrl.createDeck("Original"));
        QVERIFY(ctrl.renameDeck("Original", "Renamed"));
    }

    // Reload from disk and verify the new name persisted
    DeckController ctrl2(tmp.path());
    QCOMPARE(ctrl2.getDecks().size(), 1u);
    QCOMPARE(ctrl2.getDecks()[0].deckName, "Renamed");
}

// -----------------------------------------------------------------------

void TestDeckController::persistence_deckSurvivesReload() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    {
        DeckController ctrl(tmp.path());
        QVERIFY(ctrl.createDeck("Persistent"));
        QVERIFY(ctrl.createDeck("Also Persistent"));
    }

    DeckController ctrl2(tmp.path());
    QCOMPARE(ctrl2.getDecks().size(), 2u);

    bool foundFirst = false;
    bool foundSecond = false;
    for (const auto& deck : ctrl2.getDecks()) {
        if (deck.deckName == "Persistent")      foundFirst = true;
        if (deck.deckName == "Also Persistent") foundSecond = true;
    }
    QVERIFY(foundFirst);
    QVERIFY(foundSecond);
}

QTEST_MAIN(TestDeckController)
#include "TestDeckController.moc"
