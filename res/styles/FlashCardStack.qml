import QtQuick

Item {
    id: root

    // keep an internal property and bind it to the context property if it exists.
    property string cardsJsonInternal: ""

    Binding {
        target: root
        property: "cardsJsonInternal"
        value: (typeof cardsJson !== "undefined") ? cardsJson : cardsJsonInternal
        when: (typeof cardsJson !== "undefined")
    }

    property int currentIndex: 0
    property int pendingIndex: 0
    property int requestedIndex: 0
    property int nextRequest: 0

    property bool flipped: false
    property bool switching: false
    property real switchProgress: 0
    property bool flipInputEnabled: true

    readonly property int cardCount: cardsModel.count
    readonly property int nextIndex: switching ? pendingIndex : normalizedIndex(currentIndex + 1)

    signal cardChanged(int currentIndex, int cardCount)

    // Always show a newly selected card with its front side.
    onCurrentIndexChanged: {
        flipped = false
    }

    ListModel {
        id: cardsModel

        ListElement {
            front: "正面\n\n点击卡片查看答案"
            back: "反面\n\n这里显示答案、解释或例句"
        }
    }

    function loadCardsFromJson() {
        const json = cardsJsonInternal

        if (!json || json.length === 0) {
            console.log("cardsJson is empty, using default card")
            return
        }

        try {
            const parsed = JSON.parse(json)

            if (!Array.isArray(parsed) || parsed.length <= 0) {
                console.log("cardsJson parsed, but card array is empty")
                return
            }

            cardsModel.clear()

            for (let i = 0; i < parsed.length; ++i) {
                cardsModel.append({
                    "front": parsed[i].front ? parsed[i].front : "",
                    "back": parsed[i].back ? parsed[i].back : ""
                })
            }

            currentIndex = 0
            pendingIndex = 0
            requestedIndex = 0
            nextRequest = 0
            flipped = false
            switching = false
            switchProgress = 0

            console.log("FlashCardStack loaded cardsModel:", cardsModel.count)
            console.log("FlashCardStack cardCount:", cardCount)

            cardChanged(currentIndex, cardsModel.count)
        } catch (e) {
            console.log("Failed to parse cardsJson:", e)
        }
    }


    onCardsJsonInternalChanged: {
        loadCardsFromJson()
    }

    function normalizedIndex(index) {
        if (cardsModel.count <= 0) {
            return 0
        }

        return ((index % cardsModel.count) + cardsModel.count) % cardsModel.count
    }

    function frontAt(index) {
        if (cardsModel.count <= 0) {
            return ""
        }

        return cardsModel.get(normalizedIndex(index)).front
    }

    function backAt(index) {
        if (cardsModel.count <= 0) {
            return ""
        }

        return cardsModel.get(normalizedIndex(index)).back
    }

    function switchToNextCard() {
        switchToCard(currentIndex + 1)
    }

    function switchToPreviousCard() {
        switchToCard(currentIndex - 1)
    }

    onNextRequestChanged: {
        console.log("nextRequest changed:",
            nextRequest,
            "cardsModel.count =", cardsModel.count,
            "cardCount =", cardCount)

        switchToNextCard()
    }

    onRequestedIndexChanged: {
        switchToCard(requestedIndex)
    }

    function switchToCard(index) {
        console.log(
            "switchToCard called",
            "index =", index,
            "currentIndex =", currentIndex,
            "cardsModel.count =", cardsModel.count,
            "cardCount =", cardCount,
            "switching =", switching
        )

        if (switching || cardsModel.count <= 1) {
            console.log("switch ignored")
            return
        }

        const target = normalizedIndex(index)

        if (target === currentIndex) {
            console.log("same card ignored")
            return
        }

        pendingIndex = target
        flipped = false

        flipInputEnabled = false

        switching = true
        switchProgress = 0
        drawNextCard.restart()
    }

    SequentialAnimation {
        id: drawNextCard

        NumberAnimation {
            target: root
            property: "switchProgress"
            from: 0
            to: 1
            duration: 560
            easing.type: Easing.InOutCubic
        }

        ScriptAction {
            script: {
                root.currentIndex = root.pendingIndex
                root.switchProgress = 0
                root.switching = false
                root.flipped = false
                root.flipInputEnabled = true
                root.cardChanged(root.currentIndex, root.cardCount)

                console.log("card changed to", root.currentIndex)
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
    }

    Item {
        id: stackLayer
        anchors.fill: parent

        Item {
            id: depthWrapper

            width: parent.width
            height: parent.height

            x: 70
            y: 54
            scale: 0.84
            rotation: 4
            opacity: root.cardCount > 2 ? 0.25 : 0
            z: 0
            enabled: false

            HoloFlashCard {
                anchors.fill: parent
                frontText: root.frontAt(root.currentIndex + 2)
                backText: root.backAt(root.currentIndex + 2)
                flipped: false
            }
        }

        Item {
            id: nextWrapper

            width: parent.width
            height: parent.height

            x: 46 * (1 - root.switchProgress)
            y: 34 * (1 - root.switchProgress)
            scale: 0.91 + root.switchProgress * 0.09
            rotation: -3 + root.switchProgress * 3
            opacity: root.switching && root.cardCount > 1
                ? 0.50 + root.switchProgress * 0.50
                : 0.0
            z: root.switchProgress > 0.58 ? 4 : 1
            enabled: false

            HoloFlashCard {
                anchors.fill: parent
                frontText: root.frontAt(root.nextIndex)
                backText: root.backAt(root.nextIndex)
                flipped: false
            }
        }

        Item {
            id: activeWrapper

            width: parent.width
            height: parent.height

            x: -150 * root.switchProgress
            y: -18 * root.switchProgress
            scale: 1.0 - root.switchProgress * 0.06
            rotation: -10 * root.switchProgress
            opacity: 1.0 - root.switchProgress
            z: 3
            enabled: !root.switching

            HoloFlashCard {
                id: activeCard

                anchors.fill: parent
                frontText: root.frontAt(root.currentIndex)
                backText: root.backAt(root.currentIndex)
                flipped: root.flipped

                onCardClicked: function(isFlipped) {
                    if (!root.flipInputEnabled || root.switching)
                        return
                    root.flipped = isFlipped
                }
            }
        }
    }
}
