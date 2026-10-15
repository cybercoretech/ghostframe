import QtQuick
import QtQuick.Controls

// Small selectable chip (presets, styles, formats).
Button {
    id: chip
    property bool selected: false
    property color accent: Theme.cyan
    implicitHeight: 24
    implicitWidth: Math.max(34, label.implicitWidth + 16)
    contentItem: Text {
        id: label
        text: chip.text
        color: !chip.enabled ? Theme.muted : chip.selected ? Theme.darkText : Theme.text
        font.family: Theme.mono; font.pixelSize: 9; font.bold: chip.selected
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 2
        color: chip.selected ? chip.accent : (chip.hovered ? Theme.panelHover : Theme.panel2)
        border.color: chip.selected ? chip.accent : (chip.hovered ? chip.accent : Theme.line)
    }
}
