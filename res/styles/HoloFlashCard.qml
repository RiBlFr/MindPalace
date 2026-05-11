import QtQuick

Item {
    id: root

    property string frontText: "正面\n\n点击卡片查看答案"
    property string backText: "反面\n\n隐藏中..."
    property bool flipped: false

    property real maxTilt: 12
    property real tiltX: 0
    property real tiltY: 0
    property real shineX: 0.5
    property real shineY: 0.35
    property bool hovered: false

    property real bodyOffsetX: 0
    property real bodyOffsetY: 0

    signal cardClicked(bool flipped)

    Rectangle {
        anchors.fill: parent
        color: "transparent"
    }

    Item {
        id: card
        width: Math.min(760, Math.max(220, parent.width - 48))
        height: Math.min(460, Math.max(180, parent.height - 48))

        x: (parent.width - width) / 2 + root.bodyOffsetX
        y: (parent.height - height) / 2 + root.bodyOffsetY

        property real flipAngle: root.flipped ? 180 : 0

        Behavior on flipAngle {
            NumberAnimation {
                duration: 420
                easing.type: Easing.OutCubic
            }
        }

        transform: [
            Rotation {
                origin.x: card.width / 2
                origin.y: card.height / 2
                axis.x: 1
                axis.y: 0
                axis.z: 0
                angle: root.tiltX
            },
            Rotation {
                origin.x: card.width / 2
                origin.y: card.height / 2
                axis.x: 0
                axis.y: 1
                axis.z: 0
                angle: card.flipAngle + root.tiltY
            }
        ]

        Behavior on scale {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }

        scale: root.hovered ? 1.018 : 1.0
        layer.enabled: true
        layer.smooth: true

        Rectangle {
            id: shadow
            anchors.fill: cardBody
            anchors.topMargin: 18
            radius: cardBody.radius
            color: "#25304a"
            opacity: root.hovered ? 0.22 : 0.13
        }

        Rectangle {
            id: cardBody
            anchors.fill: parent
            radius: 28
            antialiasing: true
            clip: true

            Rectangle {
                id: cardBackground
                anchors.fill: parent
                radius: parent.radius

                transform: Rotation {
                    origin.x: cardBackground.width / 2
                    origin.y: cardBackground.height / 2
                    axis.x: 0
                    axis.y: 1
                    axis.z: 0
                    angle: card.flipAngle >= 90 ? 180 : 0
                }

                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop {
                        position: 0.0
                        color: Qt.hsla((230 - 30 + root.shineX * 60) / 360, 0.66, 0.30, 1.0)
                    }
                    GradientStop {
                        position: 0.5 + (root.shineX - 0.5) * 0.4
                        color: Qt.hsla((205 - 30 + root.shineY * 60) / 360, 0.70, 0.40, 1.0)
                    }
                    GradientStop {
                        position: 1.0
                        color: Qt.hsla((179 - 30 + (1.0 - root.shineX) * 60) / 360, 0.70, 0.48, 1.0)
                    }
                }

                Repeater {
                    model: 12

                    Rectangle {
                        width: cardBackground.width * 1.45
                        height: 2
                        radius: 1
                        color: "white"
                        opacity: 0.065
                        rotation: -18
                        x: -cardBackground.width * 0.22
                        y: index * cardBackground.height / 10
                            + (root.shineX - 0.5) * 22
                    }
                }

                Canvas {
                    id: shineCanvas
                    anchors.fill: parent
                    opacity: root.hovered ? 1.0 : 0.62
                    renderTarget: Canvas.Image
                    antialiasing: true

                    function clamp(v, minValue, maxValue) {
                        return Math.max(minValue, Math.min(maxValue, v))
                    }

                    function roundedPath(ctx, x, y, w, h, r) {
                        r = Math.min(r, w / 2, h / 2)

                        ctx.beginPath()
                        ctx.moveTo(x + r, y)
                        ctx.lineTo(x + w - r, y)
                        ctx.quadraticCurveTo(x + w, y, x + w, y + r)
                        ctx.lineTo(x + w, y + h - r)
                        ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h)
                        ctx.lineTo(x + r, y + h)
                        ctx.quadraticCurveTo(x, y + h, x, y + h - r)
                        ctx.lineTo(x, y + r)
                        ctx.quadraticCurveTo(x, y, x + r, y)
                        ctx.closePath()
                    }

                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)

                        if (width <= 4 || height <= 4) {
                            return
                        }

                        const visualSx = clamp(root.shineX, 0.0, 1.0)
                        const visualSy = clamp(root.shineY, 0.0, 1.0)

                        const sx = visualSx
                        const sy = visualSy

                        const cx = sx * width
                        const cy = sy * height

                        const cornerRadius = Math.min(28, width * 0.12, height * 0.18)
                        const glowRadius = Math.max(80, Math.min(width, height) * 0.95)

                        ctx.save()

                        roundedPath(ctx, 0, 0, width, height, cornerRadius)
                        ctx.clip()

                        // 柔和主光斑
                        const radial = ctx.createRadialGradient(cx, cy, 0, cx, cy, glowRadius)
                        radial.addColorStop(0.00, "rgba(255,255,255,0.58)")
                        radial.addColorStop(0.22, "rgba(255,255,255,0.22)")
                        radial.addColorStop(0.56, "rgba(255,255,255,0.06)")
                        radial.addColorStop(1.00, "rgba(255,255,255,0.00)")

                        ctx.fillStyle = radial
                        ctx.fillRect(0, 0, width, height)

                        // 斜向扫光，小窗口下也能覆盖整个卡片
                        const sweepOffset = (sx - 0.5) * width * 0.65
                        const sweep = ctx.createLinearGradient(
                            -width * 0.35 + sweepOffset,
                            height,
                            width * 1.35 + sweepOffset,
                            0
                        )

                        sweep.addColorStop(0.00, "rgba(255,255,255,0.00)")
                        sweep.addColorStop(0.42, "rgba(255,255,255,0.00)")
                        sweep.addColorStop(0.50, "rgba(255,255,255,0.24)")
                        sweep.addColorStop(0.58, "rgba(255,255,255,0.00)")
                        sweep.addColorStop(1.00, "rgba(255,255,255,0.00)")

                        ctx.fillStyle = sweep
                        ctx.fillRect(0, 0, width, height)

                        const glass = ctx.createLinearGradient(0, 0, 0, height)
                        glass.addColorStop(0.00, "rgba(255,255,255,0.22)")
                        glass.addColorStop(0.34, "rgba(255,255,255,0.06)")
                        glass.addColorStop(1.00, "rgba(255,255,255,0.00)")

                        ctx.fillStyle = glass
                        ctx.fillRect(0, 0, width, height)

                        ctx.restore()
                    }

                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    Component.onCompleted: requestPaint()

                    Connections {
                        target: root

                        function onShineXChanged() {
                            shineCanvas.requestPaint()
                        }

                        function onShineYChanged() {
                            shineCanvas.requestPaint()
                        }

                        function onHoveredChanged() {
                            shineCanvas.requestPaint()
                        }
                    }

                    Connections {
                        target: card

                        function onFlipAngleChanged() {
                            shineCanvas.requestPaint()
                        }
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "transparent"
                border.width: 1
                border.color: "#80ffffff"
            }

            Item {
                id: frontFace
                anchors.fill: parent
                visible: card.flipAngle < 90

                Column {
                    anchors.centerIn: parent
                    width: parent.width * 0.76
                    spacing: 18

                    Text {
                        width: parent.width
                        text: root.frontText
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.WordWrap
                        font.pixelSize: 34
                        font.bold: true
                    }

                }
            }

            Item {
                id: backFace
                anchors.fill: parent
                visible: card.flipAngle >= 90

                transform: Rotation {
                    origin.x: backFace.width / 2
                    origin.y: backFace.height / 2
                    axis.x: 0
                    axis.y: 1
                    axis.z: 0
                    angle: 180
                }

                Column {
                    anchors.centerIn: parent
                    width: parent.width * 0.76
                    spacing: 18

                    Text {
                        width: parent.width
                        text: root.backText
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.WordWrap
                        font.pixelSize: 32
                        font.bold: true
                    }

                    Text {
                        width: parent.width
                        text: "再次点击回到正面"
                        color: "#fce7f3"
                        horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: 15
                        opacity: 0.9
                    }
                }
            }
        }
    }

    MouseArea {
        id: stableHoverArea

        width: parent.width
        height: parent.height
        anchors.centerIn: card

        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        z: 20

        function clamp(v, minValue, maxValue) {
            return Math.max(minValue, Math.min(maxValue, v))
        }

        function updateTilt(mouseX, mouseY) {
            const localX = mouseX - (width - card.width) / 2
            const localY = mouseY - (height - card.height) / 2

            const nx = clamp((localX / card.width - 0.5) * 2.0, -1.0, 1.0)
            const ny = clamp((localY / card.height - 0.5) * 2.0, -1.0, 1.0)

            root.tiltY = nx * root.maxTilt
            root.tiltX = -ny * root.maxTilt

            const pointerX = clamp(localX / card.width, 0.0, 1.0)
            const pointerY = clamp(localY / card.height, 0.0, 1.0)

            // 光斑放到鼠标反方向，也就是翘起 / 迎光的一侧
            root.shineX = 1.0 - pointerX
            root.shineY = 1.0 - pointerY

        }

        onEntered: {
            root.hovered = true
        }

        onExited: {
            root.hovered = false
            root.tiltX = 0
            root.tiltY = 0
            root.shineX = 0.5
            root.shineY = 0.35
        }

        onPositionChanged: function(mouse) {
            updateTilt(mouse.x, mouse.y)
        }

        onClicked: function(mouse) {
            const localX = mouse.x - (width - card.width) / 2
            const localY = mouse.y - (height - card.height) / 2

            const insideCard =
                localX >= 0 &&
                localX <= card.width &&
                localY >= 0 &&
                localY <= card.height

            if (insideCard) {
                root.cardClicked(!root.flipped)
            }
        }
    }
    Behavior on tiltX {
        NumberAnimation {
            duration: root.hovered ? 95 : 320
            easing.type: Easing.OutCubic
        }
    }

    Behavior on tiltY {
        NumberAnimation {
            duration: root.hovered ? 95 : 320
            easing.type: Easing.OutCubic
        }
    }

    Behavior on shineX {
        NumberAnimation {
            duration: root.hovered ? 80 : 280
            easing.type: Easing.OutCubic
        }
    }

    Behavior on shineY {
        NumberAnimation {
            duration: root.hovered ? 80 : 280
            easing.type: Easing.OutCubic
        }
    }

}
