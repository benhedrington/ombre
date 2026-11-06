import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1180
    height: 780
    minimumWidth: 960
    minimumHeight: 620
    visible: true
    title: "gradgen"
    color: "#0b0b10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false

    function applyCurrent() {
        if (backend.busy || backend.previews.length === 0)
            return
        backend.apply(backend.previews[grid.currentIndex])
    }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: backend.generateNew() }
    Shortcut { sequence: "S"; context: Qt.ApplicationShortcut; onActivated: backend.saveSet() }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            // Wordmark, theme, palette swatches, help.
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Label {
                    text: "gradgen"
                    color: "#ececf4"
                    font.pixelSize: 28
                    font.weight: Font.DemiBold
                }

                Label {
                    text: qsTr("gradient wallpapers · %1").arg(backend.themeName)
                    color: "#8b8ba3"
                    font.pixelSize: 13
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Row {
                    spacing: 4
                    visible: backend.paletteColors.length > 0
                    Repeater {
                        model: backend.paletteColors
                        Rectangle {
                            width: 16
                            height: 16
                            radius: 4
                            color: modelData
                            border.color: "#33ffffff"
                            border.width: 1
                        }
                    }
                }

                Button {
                    text: "?"
                    flat: true
                    onClicked: win.helpVisible = true
                }
            }

            // One row of controls: palette source, style, size, soft, count.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: controls.implicitHeight + 24
                color: "#14141c"
                radius: Math.min(10, theme.radius)
                border.color: "#1f1f2b"
                border.width: 1

                ColumnLayout {
                    id: controls
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 7

                        Label { text: qsTr("Palette"); color: "#8b8ba3"; font.pixelSize: 12 }
                        ComboBox {
                            id: paletteBox
                            focusPolicy: Qt.NoFocus
                            font.pixelSize: 12
                            Layout.preferredWidth: 105
                            model: [qsTr("Theme"), qsTr("Cool"), qsTr("Warm"), qsTr("Mono"), qsTr("Surprise")]
                            currentIndex: backend.paletteModes.indexOf(backend.paletteMode)
                            onActivated: backend.paletteMode = backend.paletteModes[index]
                        }

                        Label { text: qsTr("Style"); color: "#8b8ba3"; font.pixelSize: 12 }
                        ComboBox {
                            id: styleBox
                            focusPolicy: Qt.NoFocus
                            font.pixelSize: 12
                            Layout.preferredWidth: 105
                            model: [qsTr("Any"), qsTr("Linear"), qsTr("Glow"), qsTr("Aurora"), qsTr("Mesh")]
                            currentIndex: backend.styles.indexOf(backend.style)
                            onActivated: backend.style = backend.styles[index]
                        }

                        Label { text: qsTr("Size"); color: "#8b8ba3"; font.pixelSize: 12 }
                        ComboBox {
                            id: sizeBox
                            focusPolicy: Qt.NoFocus
                            font.pixelSize: 12
                            Layout.preferredWidth: 125
                            model: ["1920×1200", "1920×1080", "3840×2400", "3840×2160"]
                            currentIndex: backend.sizes.indexOf(backend.size)
                            onActivated: backend.size = backend.sizes[index]
                        }

                        CheckBox {
                            focusPolicy: Qt.NoFocus
                            text: qsTr("Soft")
                            checked: backend.soft
                            onToggled: backend.soft = checked
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Calmer accents, pulled toward the background")
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            focusPolicy: Qt.NoFocus
                            text: qsTr("Generate")
                            highlighted: true
                            enabled: !backend.busy
                            onClicked: backend.generateNew()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 7

                        Label { text: qsTr("Count"); color: "#8b8ba3"; font.pixelSize: 12 }
                        RowLayout {
                            spacing: 2

                            ToolButton {
                                focusPolicy: Qt.NoFocus
                                text: "−"
                                font.pixelSize: 16
                                onClicked: backend.count = Math.max(3, backend.count - 1)
                            }
                            Label {
                                text: backend.count
                                color: "#ececf4"
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                Layout.preferredWidth: 24
                            }
                            ToolButton {
                                focusPolicy: Qt.NoFocus
                                text: "+"
                                font.pixelSize: 16
                                onClicked: backend.count = Math.min(12, backend.count + 1)
                            }
                        }

                        Item { Layout.preferredWidth: 10 }

                        Label { text: qsTr("Grain"); color: "#8b8ba3"; font.pixelSize: 12 }
                        Slider {
                            id: grainSlider
                            focusPolicy: Qt.NoFocus
                            Layout.preferredWidth: 150
                            from: 0
                            to: 0.06
                            stepSize: 0.005
                            value: backend.grain
                            onMoved: backend.grain = value
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Film grain; hides colour banding. 0 is smooth.")
                        }
                        Label {
                            text: backend.grain.toFixed(3)
                            color: "#8b8ba3"
                            font.pixelSize: 11
                            Layout.preferredWidth: 34
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            text: qsTr("Space: new set · Enter: apply")
                            color: "#565f89"
                            font.pixelSize: 11
                        }
                    }
                }
            }

            // The candidates.
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#101016"
                radius: Math.min(10, theme.radius)
                border.color: "#1f1f2b"
                border.width: 1
                clip: true

                readonly property int wantedColumns: backend.previews.length > 8 ? 4
                                                    : backend.previews.length > 4 ? 3
                                                    : Math.max(1, backend.previews.length)

                GridView {
                    id: grid
                    anchors.fill: parent
                    anchors.margins: 10
                    cellWidth: Math.floor(width / parent.wantedColumns)
                    cellHeight: Math.floor(cellWidth / 1.6)
                    focus: true
                    model: backend.previews
                    cacheBuffer: 2000

                    onCountChanged: currentIndex = 0
                    Keys.onReturnPressed: win.applyCurrent()
                    Keys.onEnterPressed: win.applyCurrent()

                    delegate: Item {
                        width: grid.cellWidth
                        height: grid.cellHeight

                        Rectangle {
                            id: card
                            anchors.fill: parent
                            anchors.margins: 4
                            radius: Math.min(6, theme.radius)
                            color: "#181820"
                            border.width: grid.currentIndex === index ? 2 : 1
                            border.color: grid.currentIndex === index ? theme.accent : "#262633"
                            clip: true

                            Image {
                                anchors.fill: parent
                                source: "file://" + modelData
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: false
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "#00000000"
                                border.width: 0
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: grid.currentIndex = index
                                onDoubleClicked: backend.apply(modelData)
                                cursorShape: Qt.PointingHandCursor
                            }
                        }
                    }
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 8
                    visible: backend.previews.length === 0 && !backend.busy
                    opacity: 0.75

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "◍"
                        color: theme.accent
                        font.pixelSize: 48
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("No wallpapers yet")
                        color: "#c8c8d8"
                        font.pixelSize: 18
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Press Space to generate a set from this theme")
                        color: "#8b8ba3"
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    visible: backend.busy
                    color: "#990b0b10"

                    Column {
                        anchors.centerIn: parent
                        spacing: 12
                        BusyIndicator {
                            anchors.horizontalCenter: parent.horizontalCenter
                            running: backend.busy
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: backend.status
                            color: "#c8c8d8"
                        }
                    }
                }
            }

            // Status and the two actions that leave the app.
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Label {
                    Layout.fillWidth: true
                    text: backend.status
                    color: "#8b8ba3"
                    elide: Text.ElideRight
                }

                Button {
                    focusPolicy: Qt.NoFocus
                    text: qsTr("Save set")
                    enabled: backend.previews.length > 0 && !backend.busy
                    onClicked: backend.saveSet()
                }

                Button {
                    focusPolicy: Qt.NoFocus
                    text: qsTr("Apply")
                    highlighted: true
                    enabled: backend.previews.length > 0 && !backend.busy
                    onClicked: win.applyCurrent()
                }
            }
        }

        // Shortcuts overlay.
        Rectangle {
            anchors.fill: parent
            color: "#cc000000"
            visible: win.helpVisible
            z: 10

            MouseArea {
                anchors.fill: parent
                onClicked: win.helpVisible = false
            }

            Rectangle {
                anchors.centerIn: parent
                width: 440
                height: 330
                color: "#17171f"
                radius: Math.min(10, theme.radius)
                border.color: "#2a2a38"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 10

                    Label {
                        text: qsTr("Shortcuts")
                        color: "#ececf4"
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 24
                        rowSpacing: 8

                        Label { text: "Space"; color: theme.accent; font.family: "monospace" }
                        Label { text: qsTr("Generate a new set"); color: "#c8c8d8"; Layout.fillWidth: true }

                        Label { text: "Enter"; color: theme.accent; font.family: "monospace" }
                        Label { text: qsTr("Apply the selected wallpaper"); color: "#c8c8d8"; Layout.fillWidth: true }

                        Label { text: "S"; color: theme.accent; font.family: "monospace" }
                        Label { text: qsTr("Save the set to this theme"); color: "#c8c8d8"; Layout.fillWidth: true }

                        Label { text: "Arrows"; color: theme.accent; font.family: "monospace" }
                        Label { text: qsTr("Move between candidates"); color: "#c8c8d8"; Layout.fillWidth: true }

                        Label { text: "?"; color: theme.accent; font.family: "monospace" }
                        Label { text: qsTr("This list"); color: "#c8c8d8"; Layout.fillWidth: true }

                        Label { text: "Q"; color: theme.accent; font.family: "monospace" }
                        Label { text: qsTr("Quit"); color: "#c8c8d8"; Layout.fillWidth: true }
                    }

                    Item { Layout.fillHeight: true }

                    Button {
                        text: qsTr("Close")
                        Layout.alignment: Qt.AlignRight
                        onClicked: win.helpVisible = false
                    }
                }
            }
        }
    }
}
