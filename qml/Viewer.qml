import QtQuick
import QtQuick.Controls

// Full-screen view of one photo, with the way into the editor.
Rectangle {
    id: viewer
    property var photo: null
    signal closeRequested()
    signal editRequested()

    color: "#e6080a0d"
    visible: photo !== null

    Image {
        anchors.fill: parent; anchors.margins: 54
        source: viewer.photo ? viewer.photo.source : ""
        fillMode: Image.PreserveAspectFit; asynchronous: true
    }
    Button {
        id: editButton
        anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 20
        text: imageEditor.busy ? "PREPARING…" : "✎  EDIT IMAGE"
        enabled: !imageEditor.busy
        onClicked: viewer.editRequested()
        contentItem: Text { text: editButton.text; color: Theme.darkText; font.family: Theme.mono; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { color: editButton.hovered ? Qt.lighter(Theme.cyan, 1.1) : Theme.cyan; border.color: Theme.darkText; radius: 2 }
    }
    Rectangle {
        anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 20
        width: 42; height: 42; color: Theme.panel; border.color: Theme.pink
        Accessible.role: Accessible.Button
        Accessible.name: "Close photo"
        Text { anchors.centerIn: parent; text: "×"; color: Theme.text; font.pixelSize: 25 }
        MouseArea { anchors.fill: parent; onClicked: viewer.closeRequested() }
    }
    Text {
        anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 24
        text: viewer.photo ? viewer.photo.title + "   //   " + viewer.photo.subtitle : ""
        color: Theme.text; font.family: Theme.mono; font.pixelSize: 12
    }
}
