import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Labelled adjustment slider. `moved` fires live while dragging; `committed`
// fires once when the drag ends, which becomes a single undo step.
ColumnLayout {
    id: adjust
    property string label
    property string suffix: ""
    property real from: -50
    property real to: 50
    property real value: 0
    property color accent: Theme.cyan
    readonly property real liveValue: slider.value
    signal moved(real value)
    signal committed()

    Layout.fillWidth: true
    spacing: 2
    RowLayout {
        Layout.fillWidth: true
        Text { text: adjust.label; color: Theme.text; font.family: Theme.mono; font.pixelSize: 10; Layout.fillWidth: true }
        Text { text: Math.round(slider.value) + adjust.suffix; color: adjust.accent; font.family: Theme.mono; font.pixelSize: 10 }
    }
    Slider {
        id: slider
        Layout.fillWidth: true
        from: adjust.from; to: adjust.to; value: adjust.value
        onMoved: adjust.moved(value)
        onPressedChanged: if (!pressed) adjust.committed()
        background: Rectangle {
            x: slider.leftPadding; y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth; height: 3; radius: 1; color: Theme.line
            Rectangle { width: slider.visualPosition * parent.width; height: parent.height; radius: 1; color: adjust.accent }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 12; height: 12; radius: 2
            color: slider.pressed ? adjust.accent : Theme.panel
            border.color: adjust.accent; border.width: 2
        }
    }
}
