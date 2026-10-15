import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Text and watermark layers. Sizes and margins scale with the image, so a
// layer looks the same in the preview and in any export size.
ColumnLayout {
    id: panel
    readonly property var layers: imageEditor.recipe.textLayers || []
    readonly property var swatches: ["35f4ff", "ff36a8", "b4f33a", "edf1f4", "080a0d", "ffb347"]

    Layout.fillWidth: true
    spacing: 8

    Repeater {
        model: panel.layers.length
        delegate: Rectangle {
            id: card
            required property int index
            readonly property var entry: panel.layers[index] || ({})
            Layout.fillWidth: true
            implicitHeight: body.implicitHeight + 16
            color: Theme.bg; border.color: Theme.line; radius: 2
            ColumnLayout {
                id: body
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8
                spacing: 5
                RowLayout {
                    Layout.fillWidth: true
                    MetaField {
                        Layout.fillWidth: true; text: card.entry.text || ""
                        onEditingFinished: if (text !== card.entry.text) imageEditor.setTextLayer(card.index, "text", text, true)
                    }
                    Chip { text: "×"; implicitWidth: 24; accent: Theme.pink; onClicked: imageEditor.removeTextLayer(card.index) }
                }
                RowLayout {
                    spacing: 4
                    Repeater {
                        model: panel.swatches
                        delegate: Rectangle {
                            required property string modelData
                            width: 20; height: 16; radius: 2; color: "#" + modelData
                            border.color: card.entry.color === modelData ? Theme.text : Theme.line; border.width: card.entry.color === modelData ? 2 : 1
                            MouseArea { anchors.fill: parent; onClicked: imageEditor.setTextLayer(card.index, "color", parent.modelData, true) }
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Chip { text: "B"; selected: card.entry.bold === true; onClicked: imageEditor.setTextLayer(card.index, "bold", !card.entry.bold, true) }
                }
                RowLayout {
                    Layout.fillWidth: true
                    // 3×3 anchor grid.
                    Grid {
                        columns: 3; spacing: 2
                        Repeater {
                            model: 9
                            delegate: Rectangle {
                                required property int index
                                width: 14; height: 10; radius: 1
                                color: card.entry.anchor === index ? Theme.cyan : Theme.panel2
                                border.color: Theme.line
                                MouseArea { anchors.fill: parent; onClicked: imageEditor.setTextLayer(card.index, "anchor", parent.index, true) }
                            }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 0
                        LayerSlider { label: "SIZE"; field: "size"; from: 0.01; to: 0.3; value: card.entry.size || 0.06; scaleShown: 100; suffix: "%" }
                        LayerSlider { label: "MARGIN"; field: "margin"; from: 0; to: 0.2; value: card.entry.margin || 0; scaleShown: 100; suffix: "%" }
                    }
                }
                LayerSlider { label: "OPACITY"; field: "opacity"; from: 0; to: 1; value: card.entry.opacity === undefined ? 1 : card.entry.opacity; scaleShown: 100; suffix: "%" }
                LayerSlider { label: "NEON GLOW"; field: "glow"; from: 0; to: 100; value: card.entry.glow || 0 }
            }
            component LayerSlider: RowLayout {
                id: ls
                property string label
                property string field
                property real from: 0
                property real to: 1
                property real value: 0
                property real scaleShown: 1
                property string suffix: ""
                Layout.fillWidth: true
                Text { text: ls.label; color: Theme.muted; font.family: Theme.mono; font.pixelSize: 9; Layout.preferredWidth: 62 }
                MiniSlider {
                    id: s
                    Layout.fillWidth: true; from: ls.from; to: ls.to; value: ls.value
                    onMoved: imageEditor.setTextLayer(card.index, ls.field, value, false)
                    onPressedChanged: if (!pressed) imageEditor.commitEdit("Text " + ls.field)
                }
                Text { text: Math.round(s.value * ls.scaleShown) + ls.suffix; color: Theme.cyan; font.family: Theme.mono; font.pixelSize: 9; Layout.preferredWidth: 32 }
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        EditorButton { text: "+ TEXT"; Layout.fillWidth: true; enabled: imageEditor.ready && panel.layers.length < 16; onClicked: imageEditor.addTextLayer("GHOSTFRAME") }
        EditorButton {
            text: "+ WATERMARK"; primary: false; Layout.fillWidth: true; enabled: imageEditor.ready && panel.layers.length < 16
            onClicked: {
                imageEditor.addTextLayer("© NEON COFFIN VIDEO")
                const i = panel.layers.length - 1
                imageEditor.setTextLayer(i, "size", 0.028, false)
                imageEditor.setTextLayer(i, "opacity", 0.75, false)
                imageEditor.setTextLayer(i, "color", "edf1f4", false)
                imageEditor.setTextLayer(i, "glow", 25, true)
            }
        }
    }
}
