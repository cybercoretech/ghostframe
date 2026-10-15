import QtQuick
import QtQuick.Layouts

// Thin status strip across the top of the window.
Rectangle {
    color: Theme.bg
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
    RowLayout {
        anchors.fill: parent; anchors.leftMargin: 22; anchors.rightMargin: 22
        Text { text: "▮  GHOSTFRAME // LOCAL PHOTO ARCHIVE"; color: Theme.lime; font.family: Theme.mono; font.pixelSize: 10; font.letterSpacing: 1.5 }
        Item { Layout.fillWidth: true }
        Text {
            text: "PHOTO INDEX  ·  " + (photoService.connected ? "ONLINE" : "OFFLINE")
            color: photoService.connected ? Theme.cyan : Theme.muted
            font.family: Theme.mono; font.pixelSize: 10; font.letterSpacing: 1.2
        }
        Text { text: "   NODE 01"; color: Theme.pink; font.family: Theme.mono; font.pixelSize: 10; font.letterSpacing: 1.2 }
    }
}
