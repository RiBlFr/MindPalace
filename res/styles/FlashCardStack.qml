import QtQuick

Item {
    id: root
    
    readonly property real minSide: Math.max(1, Math.min(width, height))
    readonly property real stackPad: Math.max(10, Math.min(70, minSide * 0.12))
    readonly property real depthScaleBase: Math.max(0.78, Math.min(0.88, 0.84 * (minSide / 520)))
    readonly property real nextScaleBase: Math.max(0.86, Math.min(0.95, 0.91 * (minSide / 520)))
    readonly property real showBackCardThreshold: 260
    readonly property real backCardShiftX: root.stackPad * 1.05
    readonly property real backCardShiftY: root.stackPad * 0.80
    readonly property real backCardBodyOffsetX: root.stackPad * 0.90
    readonly property real backCardBodyOffsetY: root.stackPad * 0.70

    readonly property real nextScaleMin: root.nextScaleBase - 0.03
    readonly property real maxRotation: Math.min(2.5, root.stackPad * 0.045)

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
    property real switchPhase: 0
    property real bgFadeInPhase: 1.0
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
                    "front": parsed[i].front || "",
                    "back": parsed[i].back || ""
                })
            }

            currentIndex = 0
            pendingIndex = 0
            requestedIndex = 0
            nextRequest = 0
            flipped = false
            switching = false
            switchPhase = 0

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
        switchPhase = 0
        bgFadeInPhase = 0.0
        drawNextCard.restart()
    }

    SequentialAnimation {
        id: drawNextCard
        
        NumberAnimation {
            target: root
            property: "switchPhase"
            from: 0
            to: 1
            duration: 560
            easing.type: Easing.InOutCubic
        }

        ScriptAction {
            script: {
                root.currentIndex = root.pendingIndex
                root.flipped = false
                root.switching = false
                root.flipInputEnabled = true
                root.switchPhase = 0

                root.cardChanged(root.currentIndex, root.cardCount)
                console.log("switch finished")
            }
        }

        NumberAnimation {
            target: root
            property: "bgFadeInPhase"
            from: 0.0
            to: 1.0
            duration: 250
            easing.type: Easing.OutQuad
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
            id: nextWrapper

            width: parent.width
            height: parent.height
            anchors.centerIn: parent
            x: root.backCardShiftX * (1 - root.switchPhase)
            y: root.backCardShiftY * (1 - root.switchPhase)
            scale: root.nextScaleMin + root.switchPhase * (1.0 - root.nextScaleMin)
            rotation: -root.maxRotation + root.switchPhase * root.maxRotation
            opacity: (root.cardCount > 1 && root.minSide >= root.showBackCardThreshold)
                ? (root.switching ? (0.55 + root.switchPhase * 0.45) : (0.35 * root.bgFadeInPhase))
                : 0.0
            z: 1
            enabled: false

            HoloFlashCard {
                anchors.fill: parent
                frontText: root.frontAt(root.nextIndex)
                backText: root.backAt(root.nextIndex)
                flipped: false
                bodyOffsetX: root.backCardBodyOffsetX * (1 - root.switchPhase)
                bodyOffsetY: root.backCardBodyOffsetY * (1 - root.switchPhase)
            }
        }

        Item {
            id: activeWrapper

            width: parent.width
            height: parent.height
            anchors.centerIn: parent
            x: root.backCardShiftX * root.switchPhase
            y: root.backCardShiftY * root.switchPhase
            scale: 1.0 - root.switchPhase * (1.0 - root.nextScaleMin)
            rotation: -root.maxRotation * root.switchPhase
            opacity: 1.0 - root.switchPhase
            z: 3
            enabled: !root.switching

            HoloFlashCard {
                id: activeCard

                anchors.fill: parent
                frontText: root.frontAt(root.currentIndex)
                backText: root.backAt(root.currentIndex)
                flipped: root.flipped

                bodyOffsetX: root.backCardBodyOffsetX * root.switchPhase
                bodyOffsetY: root.backCardBodyOffsetY * root.switchPhase

                onCardClicked: function(isFlipped) {
                    if (!root.flipInputEnabled || root.switching)
                        return
                    root.flipped = isFlipped
                }
            }
        }
    }
}
