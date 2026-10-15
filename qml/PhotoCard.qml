import QtQuick

// One photo in the grid: cyan frame at rest, pink on hover.
Item {
    id: card
    required property string title
    required property string subtitle
    required property url thumbnail
    required property url fullImage
    required property string hash
    required property string folder
    required property string originalName
    required property int index
    signal opened(var photo)

    Rectangle {
        anchors.fill: parent
        color: Theme.panel
        border.color: mouse.containsMouse ? Theme.pink : Theme.alpha(Theme.cyan, 0.55)
        border.width: mouse.containsMouse ? 2 : 1
        radius: 2
        Rectangle {
            anchors.fill: parent; anchors.margins: 3
            color: Theme.bg; radius: 2; clip: true
            Image {
                id: thumb
                anchors.fill: parent; anchors.margins: 1
                source: card.thumbnail; fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: true
                Rectangle {
                    anchors.fill: parent; color: Theme.panel2; visible: thumb.status !== Image.Ready
                    Text { anchors.centerIn: parent; text: thumb.status === Image.Error ? "IMAGE UNAVAILABLE" : "◌  LOADING"; color: Theme.muted; font.family: Theme.mono; font.pixelSize: 9 }
                }
            }
            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 48; color: "#f20a0e13" }
            Rectangle { anchors.left: parent.left; anchors.bottom: parent.bottom; width: mouse.containsMouse ? 4 : 2; height: 48; color: mouse.containsMouse ? Theme.pink : Theme.cyan }
            Column {
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 9; spacing: 3
                Text { width: parent.width; text: card.title; color: Theme.text; font.family: Theme.sans; font.pixelSize: 11; elide: Text.ElideRight }
                Text { width: parent.width; text: card.subtitle; color: Theme.cyan; font.family: Theme.mono; font.pixelSize: 9; elide: Text.ElideRight }
            }
        }
        MouseArea {
            id: mouse
            anchors.fill: parent; hoverEnabled: true
            onClicked: card.opened(photoService.photoAt(card.index))
        }
    }
}
