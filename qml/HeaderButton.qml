import QtQuick
import QtQuick.Controls

// Solid neon button used in the header.
Button {
    id: control
    property color accent: Theme.cyan
    property bool selected: false
    contentItem: Text {
        text: control.text; color: Theme.darkText
        font.family: Theme.mono; font.pixelSize: 10; font.bold: true; font.letterSpacing: 0.4
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        color: control.hovered ? Qt.lighter(control.accent, 1.14) : control.accent
        border.color: control.selected ? Theme.text : Theme.darkText
        border.width: control.selected ? 2 : 1
        radius: 1
    }
}
