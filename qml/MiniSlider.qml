import QtQuick
import QtQuick.Controls

// Compact slider in the Ghostframe style (text layers, export quality).
Slider {
    id: control
    property color accent: Theme.cyan
    implicitHeight: 18
    background: Rectangle {
        x: control.leftPadding; y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth; height: 3; radius: 1; color: Theme.line
        Rectangle { width: control.visualPosition * parent.width; height: parent.height; radius: 1; color: control.accent }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: 10; height: 10; radius: 2
        color: control.pressed ? control.accent : Theme.panel
        border.color: control.accent; border.width: 2
    }
}
