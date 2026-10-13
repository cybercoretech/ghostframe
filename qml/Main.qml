import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1440
    height: 940
    minimumWidth: 900
    minimumHeight: 620
    visible: true
    title: "GHOSTFRAME // Cybercore Photo Archive"
    color: palette.bg

    QtObject {
        id: palette
        readonly property color bg: "#080a0d"
        readonly property color panel: "#0e1217"
        readonly property color panel2: "#151b22"
        readonly property color line: "#29323b"
        readonly property color text: "#e8edf1"
        readonly property color muted: "#77838e"
        readonly property color cyan: "#27e8ef"
        readonly property color pink: "#ff2c9c"
        readonly property color lime: "#b8ff3b"
    }

    property var selectedPhoto: null
    property bool showSettings: !photoService.connected
    property string savedUrl: "http://127.0.0.1:2342"

    Shortcut { sequence: "Ctrl+K"; onActivated: searchField.forceActiveFocus() }
    Shortcut { sequence: "Escape"; onActivated: { selectedPhoto = null; showSettings = false } }
    Shortcut { sequence: "Ctrl+R"; onActivated: photoService.search(searchField.text) }
    Connections { target: photoService; function onConnectedChanged() { if (photoService.connected) root.showSettings = false } }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: palette.bg
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: palette.line }
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 22; anchors.rightMargin: 22
                Text { text: "▮  GHOSTFRAME // LOCAL PHOTO ARCHIVE"; color: palette.lime; font.family: "JetBrains Mono"; font.pixelSize: 10; font.letterSpacing: 1.5 }
                Item { Layout.fillWidth: true }
                Text { text: "PHOTO INDEX  ·  " + (photoService.connected ? "ONLINE" : "OFFLINE"); color: photoService.connected ? palette.cyan : palette.muted; font.family: "JetBrains Mono"; font.pixelSize: 10; font.letterSpacing: 1.2 }
                Text { text: "   NODE 01"; color: palette.pink; font.family: "JetBrains Mono"; font.pixelSize: 10; font.letterSpacing: 1.2 }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 72
            color: palette.bg
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: palette.line }
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 22; anchors.rightMargin: 22; spacing: 9
                Column {
                    Layout.preferredWidth: 265; spacing: 0
                    Text { text: "GHOSTFRAME"; color: palette.text; font.family: "JetBrains Mono"; font.pixelSize: 26; font.bold: true; font.letterSpacing: 1 }
                    Text { text: "CYBERCORE PHOTO SYSTEMS"; color: palette.lime; font.family: "JetBrains Mono"; font.pixelSize: 9; font.letterSpacing: 2 }
                }
                Item { Layout.fillWidth: true }
                TextField {
                    id: searchField
                    Layout.preferredWidth: 250
                    Layout.preferredHeight: 42
                    placeholderText: "⌕  Search your archive  Ctrl+K"
                    color: palette.text; placeholderTextColor: palette.muted
                    font.family: "JetBrains Mono"; font.pixelSize: 12
                    background: Rectangle { color: palette.panel; border.color: searchField.activeFocus ? palette.cyan : palette.cyan; border.width: 1; radius: 2 }
                    onAccepted: photoService.search(text)
                }
                HeaderButton { label: "▦  LIBRARY"; accent: palette.pink; active: !showSettings; onClicked: { showSettings = false; selectedPhoto = null } }
                HeaderButton { label: "⌘  SOURCE"; accent: palette.cyan; active: false; onClicked: Qt.openUrlExternally("https://github.com/cybercore-tech/ghostframe") }
                HeaderButton { label: "⚙  SETTINGS"; accent: palette.lime; active: showSettings; onClicked: showSettings = !showSettings }
            }
        }

        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            Rectangle {
                Layout.preferredWidth: 210; Layout.fillHeight: true; color: palette.panel
                Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: palette.line }
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 18; spacing: 15
                    Text { text: "◈  COLLECTIONS"; color: palette.lime; font.family: "JetBrains Mono"; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1 }
                    SideItem { icon: "▦"; label: "All Images"; selected: !showSettings; accent: palette.cyan; onClicked: { showSettings = false; selectedPhoto = null } }
                    SideItem { icon: "◷"; label: "Recently Added"; selected: false; accent: palette.pink; onClicked: { showSettings = false; photoService.search("") } }
                    SideItem { icon: "☆"; label: "Favorites"; selected: false; accent: palette.lime; onClicked: { showSettings = false; photoService.search("favorite:true") } }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: palette.line; Layout.topMargin: 5; Layout.bottomMargin: 3 }
                    Text { text: "◉  LIBRARY STATUS"; color: palette.muted; font.family: "JetBrains Mono"; font.pixelSize: 10; font.letterSpacing: 1 }
                    Text { text: photoService.connected ? photoService.photoCount + " indexed items" : "No active connection"; color: palette.text; font.family: "JetBrains Mono"; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    Item { Layout.fillHeight: true }
                    Text { text: "LOCAL-FIRST  /  ORIGINALS STAY YOURS"; color: palette.muted; font.family: "JetBrains Mono"; font.pixelSize: 8; wrapMode: Text.Wrap; Layout.fillWidth: true }
                }
            }

            Item {
                Layout.fillWidth: true; Layout.fillHeight: true
                GridView {
                    id: grid
                    anchors.fill: parent; anchors.margins: 20; cellWidth: 216; cellHeight: 194
                    model: photoService
                    clip: true
                    cacheBuffer: 1200
                    delegate: Item {
                        required property string title
                        required property string subtitle
                        required property url thumbnail
                        required property url fullImage
                        width: grid.cellWidth - 12; height: grid.cellHeight - 12
                        Rectangle {
                            anchors.fill: parent; color: palette.panel; border.color: mouse.containsMouse ? palette.cyan : palette.line; border.width: mouse.containsMouse ? 1 : 1; radius: 2
                            Image { id: thumb; anchors.fill: parent; anchors.margins: 1; source: thumbnail; fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: true
                                Rectangle { anchors.fill: parent; color: palette.panel2; visible: thumb.status !== Image.Ready
                                    Text { anchors.centerIn: parent; text: thumb.status === Image.Error ? "IMAGE UNAVAILABLE" : "◌  LOADING"; color: palette.muted; font.family: "JetBrains Mono"; font.pixelSize: 9 }
                                }
                            }
                            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 44; color: "#e6080a0d" }
                            Column { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 9; spacing: 3
                                Text { width: parent.width; text: title; color: palette.text; font.family: "Inter"; font.pixelSize: 11; elide: Text.ElideRight }
                                Text { width: parent.width; text: subtitle; color: palette.cyan; font.family: "JetBrains Mono"; font.pixelSize: 9; elide: Text.ElideRight }
                            }
                            MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: root.selectedPhoto = ({title: title, subtitle: subtitle, source: fullImage}) }
                        }
                    }
                    onAtYEndChanged: if (atYEnd) photoService.loadMore()
                    ScrollBar.vertical: ScrollBar { active: true }
                }
                Column {
                    anchors.centerIn: parent; width: Math.min(parent.width - 50, 500); spacing: 12; visible: showSettings || photoService.photoCount === 0
                    Text { text: showSettings ? "◈  CONNECT YOUR ARCHIVE" : "⌁  NO IMAGES IN VIEW"; color: palette.lime; font.family: "JetBrains Mono"; font.pixelSize: 17; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }
                    Text { text: showSettings ? "Ghostframe is your custom local browser. Connect it to a PhotoPrism instance to see your images." : photoService.status; color: palette.muted; font.family: "Inter"; font.pixelSize: 13; wrapMode: Text.Wrap; width: parent.width; horizontalAlignment: Text.AlignHCenter }
                    Rectangle {
                        width: parent.width; height: 226; color: palette.panel; border.color: palette.line; radius: 2
                        ColumnLayout { anchors.fill: parent; anchors.margins: 18; spacing: 10
                            Text { text: "PHOTOPRISM ENDPOINT"; color: palette.cyan; font.family: "JetBrains Mono"; font.pixelSize: 10; font.letterSpacing: 1 }
                            TextField { id: serverField; Layout.fillWidth: true; text: root.savedUrl; placeholderText: "http://127.0.0.1:2342"; color: palette.text; font.family: "JetBrains Mono"; background: Rectangle { color: palette.bg; border.color: palette.line; border.width: 1; radius: 2 } }
                            Text { text: "APP PASSWORD / SCOPED API TOKEN"; color: palette.cyan; font.family: "JetBrains Mono"; font.pixelSize: 10; font.letterSpacing: 1 }
                            TextField { id: tokenField; Layout.fillWidth: true; placeholderText: "Token stays in memory for this session"; echoMode: TextInput.Password; color: palette.text; font.family: "JetBrains Mono"; background: Rectangle { color: palette.bg; border.color: palette.line; border.width: 1; radius: 2 } }
                            RowLayout { Layout.fillWidth: true
                                Text { Layout.fillWidth: true; text: photoService.status; color: palette.muted; font.family: "Inter"; font.pixelSize: 10; wrapMode: Text.Wrap }
                                Button { text: "CONNECT  →"; onClicked: { root.savedUrl = serverField.text; photoService.connectToServer(serverField.text, tokenField.text); tokenField.clear() }
                                    contentItem: Text { text: parent.text; color: palette.bg; font.family: "JetBrains Mono"; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: palette.lime; radius: 2 }
                                }
                            }
                        }
                    }
                }
                Rectangle {
                    anchors.fill: parent; color: "#e6080a0d"; visible: root.selectedPhoto !== null; z: 3
                    Image { anchors.fill: parent; anchors.margins: 54; source: root.selectedPhoto ? root.selectedPhoto.source : ""; fillMode: Image.PreserveAspectFit; asynchronous: true }
                    Rectangle { anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 20; width: 42; height: 42; color: palette.panel; border.color: palette.pink
                        Text { anchors.centerIn: parent; text: "×"; color: palette.text; font.pixelSize: 25 }
                        MouseArea { anchors.fill: parent; onClicked: root.selectedPhoto = null }
                    }
                    Text { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 24; text: root.selectedPhoto ? root.selectedPhoto.title + "   //   " + root.selectedPhoto.subtitle : ""; color: palette.text; font.family: "JetBrains Mono"; font.pixelSize: 12 }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 30; color: palette.panel
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: palette.line }
            RowLayout { anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 18
                Text { text: "◆  " + photoService.status; color: palette.muted; font.family: "JetBrains Mono"; font.pixelSize: 9; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: "GHOSTFRAME  /  v0.1.0"; color: palette.lime; font.family: "JetBrains Mono"; font.pixelSize: 9; letterSpacing: 1 }
            }
        }
    }

    component HeaderButton: Button {
        property color accent: palette.cyan
        property bool active: false
        contentItem: Text { text: parent.text; color: parent.active ? palette.bg : palette.text; font.family: "JetBrains Mono"; font.pixelSize: 10; font.bold: true; letterSpacing: 0.4; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { color: parent.active ? parent.accent : palette.bg; border.color: parent.accent; border.width: 1; radius: 1 }
    }
    component SideItem: Item {
        property string icon
        property string label
        property bool selected: false
        property color accent: palette.cyan
        signal clicked()
        Layout.fillWidth: true; Layout.preferredHeight: 32
        Rectangle { anchors.fill: parent; color: selected ? palette.panel2 : "transparent"; border.color: selected ? accent : "transparent"; border.width: 1; radius: 2 }
        Row { anchors.fill: parent; anchors.leftMargin: 10; spacing: 10
            Text { text: icon; color: accent; font.family: "JetBrains Mono"; font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter }
            Text { text: label; color: selected ? palette.text : palette.muted; font.family: "Inter"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
        }
        MouseArea { anchors.fill: parent; onClicked: parent.clicked() }
    }
}
