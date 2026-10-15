import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Collections and the PhotoPrism folder tree.
Rectangle {
    id: sidebar
    property bool allImagesSelected: false
    signal allImagesClicked()
    signal searchClicked(string query)
    signal folderClicked(string path)

    color: Theme.panel
    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.line }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 18; spacing: 15
        Text { text: "◈  COLLECTIONS"; color: Theme.text; font.family: Theme.mono; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1 }
        SideItem { icon: "▦"; label: "All Images"; accent: Theme.cyan; selected: sidebar.allImagesSelected; onClicked: sidebar.allImagesClicked() }
        SideItem { icon: "◷"; label: "Recently Added"; accent: Theme.pink; onClicked: sidebar.searchClicked("order:newest") }
        SideItem { icon: "☆"; label: "Favorites"; accent: Theme.lime; onClicked: sidebar.searchClicked("favorite:true") }
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line; Layout.topMargin: 2; Layout.bottomMargin: 1 }
        RowLayout {
            Layout.fillWidth: true
            Text { text: "▤  FOLDERS"; color: Theme.lime; font.family: Theme.mono; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1; Layout.fillWidth: true }
            ToolButton {
                id: folderRefresh
                text: "↻"
                Accessible.name: "Refresh folders"
                onClicked: photoService.refreshFolders()
                background: Rectangle { color: folderRefresh.hovered ? Theme.panel2 : "transparent" }
                contentItem: Text { text: folderRefresh.text; color: Theme.muted; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }
        ListView {
            id: folderList
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 90
            model: photoService.folders
            clip: true
            spacing: 2
            ScrollBar.vertical: ScrollBar { active: true }
            delegate: Item {
                id: row
                required property var modelData
                required property int index
                readonly property bool current: photoService.selectedFolder === modelData.path
                readonly property color accent: Theme.accentAt(index)
                width: folderList.width
                height: 30
                Rectangle {
                    anchors.fill: parent; radius: 2
                    color: row.current ? Theme.alpha(row.accent, 0.22) : rowHover.containsMouse ? Theme.alpha(row.accent, 0.30) : "#10161d"
                    border.color: row.current || rowHover.containsMouse ? row.accent : Theme.line
                    border.width: row.current || rowHover.containsMouse ? 2 : 0
                }
                Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: rowHover.containsMouse || row.current ? 4 : 3; color: row.accent }
                Text {
                    anchors.fill: parent; anchors.leftMargin: 12 + row.modelData.depth * 10; anchors.rightMargin: 8
                    verticalAlignment: Text.AlignVCenter
                    text: "▸  " + row.modelData.title
                    color: row.current ? Theme.text : row.accent
                    font.family: Theme.sans; font.pixelSize: 11; elide: Text.ElideMiddle
                }
                MouseArea { id: rowHover; anchors.fill: parent; hoverEnabled: true; onClicked: sidebar.folderClicked(row.modelData.path) }
            }
        }
        Text {
            text: photoService.connected ? photoService.photoCount + " indexed items" : "Connect PhotoPrism to browse"
            color: Theme.muted; font.family: Theme.mono; font.pixelSize: 9; wrapMode: Text.Wrap; Layout.fillWidth: true
        }
    }
}
