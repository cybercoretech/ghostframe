import QtQuick
import QtQuick.Layouts

// One entry in the sidebar's collections list.
Item {
    id: item
    property string icon
    property string label
    property bool selected: false
    property color accent: Theme.cyan
    signal clicked()

    Layout.fillWidth: true
    Layout.preferredHeight: 32

    Rectangle {
        anchors.fill: parent; radius: 2; border.width: 1
        color: item.selected ? Theme.panel2 : (hover.containsMouse ? Theme.panelHover : "transparent")
        border.color: item.selected ? item.accent : (hover.containsMouse ? Theme.line : "transparent")
    }
    Row {
        anchors.fill: parent; anchors.leftMargin: 10; spacing: 10
        Text { text: item.icon; color: item.accent; font.family: Theme.mono; font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter }
        Text {
            text: item.label
            color: item.selected ? Theme.text : (hover.containsMouse ? item.accent : Theme.muted)
            font.family: Theme.sans; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter
        }
    }
    MouseArea { id: hover; anchors.fill: parent; hoverEnabled: true; onClicked: item.clicked() }
}
