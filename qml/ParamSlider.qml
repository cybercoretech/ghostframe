import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A slider bound to one recipe parameter (`key`). Dragging previews live
// (draft quality); releasing records one undo step. Double-click the label
// to reset just this parameter.
ColumnLayout {
    id: param
    property string key
    property string label
    property real from: -100
    property real to: 100
    property real stepSize: 1
    property real defaultValue: 0
    property int decimals: 0
    property real displayScale: 1    // shown value = value * displayScale
    property string suffix: ""
    property color accent: Theme.cyan
    readonly property real value: imageEditor.recipe[key] !== undefined ? imageEditor.recipe[key] : defaultValue
    readonly property bool changed: Math.abs(value - defaultValue) > 1e-6

    Layout.fillWidth: true
    spacing: 1
    enabled: imageEditor.ready

    RowLayout {
        Layout.fillWidth: true
        Text {
            text: param.label
            color: param.changed ? Theme.text : Theme.muted
            font.family: Theme.mono; font.pixelSize: 10
            Layout.fillWidth: true
            MouseArea {
                anchors.fill: parent
                onDoubleClicked: imageEditor.applyParameter(param.key, param.defaultValue, "Reset " + param.label.toLowerCase())
                ToolTip.visible: containsMouse && param.changed; ToolTip.text: "Double-click to reset"; hoverEnabled: true
            }
        }
        Text {
            text: (slider.value * param.displayScale).toFixed(param.decimals) + param.suffix
            color: param.changed ? param.accent : Theme.muted
            font.family: Theme.mono; font.pixelSize: 10
        }
    }
    Slider {
        id: slider
        Layout.fillWidth: true
        from: param.from; to: param.to; stepSize: param.stepSize
        value: param.value
        onMoved: imageEditor.setParameter(param.key, value)
        onPressedChanged: if (!pressed) imageEditor.commitEdit(param.label)
        background: Rectangle {
            x: slider.leftPadding; y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth; height: 3; radius: 1; color: Theme.line
            // Fill from the default value, so a centred slider shows its direction.
            readonly property real zero: (param.defaultValue - slider.from) / (slider.to - slider.from)
            Rectangle {
                x: Math.min(parent.zero, slider.visualPosition) * parent.width
                width: Math.abs(slider.visualPosition - parent.zero) * parent.width
                height: parent.height; radius: 1; color: param.accent
            }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 12; height: 12; radius: 2
            color: slider.pressed ? param.accent : Theme.panel
            border.color: param.changed ? param.accent : Theme.muted; border.width: 2
        }
    }
}
