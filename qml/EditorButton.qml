import QtQuick
import QtQuick.Controls

// Editor side-panel button: solid accent ("primary") or outlined panel style.
Button {
    id: control
    property color accent: Theme.cyan
    property bool primary: true
    implicitHeight: 32
    contentItem: Text {
        text: control.text
        color: !control.enabled ? Theme.muted : control.primary ? Theme.darkText : Theme.text
        font.family: Theme.mono; font.pixelSize: 9; font.bold: control.primary
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 2
        color: !control.enabled ? Theme.panel2
             : control.primary ? (control.hovered ? Qt.lighter(control.accent, 1.1) : control.accent)
             : (control.hovered ? Theme.panelHover : Theme.panel2)
        border.color: control.primary ? "transparent" : (control.hovered ? control.accent : Theme.line)
        border.width: 1
    }
}
