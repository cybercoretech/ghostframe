import QtQuick
import QtQuick.Layouts

// Bottom status line: a transient notice if set, otherwise PhotoService status.
Rectangle {
    id: bar
    property string notice: ""
    color: Theme.panel
    Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.line }
    RowLayout {
        anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 18
        Text {
            text: "◆  " + (bar.notice || photoService.status)
            color: bar.notice ? Theme.cyan : Theme.muted
            font.family: Theme.mono; font.pixelSize: 9; Layout.fillWidth: true; elide: Text.ElideRight
        }
        Text { text: "GHOSTFRAME  /  v0.1.0"; color: Theme.lime; font.family: Theme.mono; font.pixelSize: 9; font.letterSpacing: 1 }
    }
}
