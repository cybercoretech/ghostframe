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
        readonly property color panel: "#0d1218"
        readonly property color panel2: "#18212a"
        readonly property color panelHover: "#202c36"
        readonly property color line: "#29323b"
        readonly property color text: "#edf1f4"
        readonly property color muted: "#c0c8d0"
        readonly property color darkText: "#101318"
        readonly property color cyan: "#35f4ff"
        readonly property color pink: "#ff36a8"
        readonly property color lime: "#b4f33a"
    }

    property var themeColors: palette
    property var selectedPhoto: null
    property bool showSettings: !photoService.connected
    property string savedUrl: "http://127.0.0.1:2342"
    property string saveNotice: ""

    Shortcut { sequence: "Ctrl+K"; onActivated: searchField.forceActiveFocus() }
    Shortcut { sequence: "Escape"; onActivated: { if (showEditor) { imageEditor.cancel(); showEditor = false } else { selectedPhoto = null; showSettings = false } } }
    Shortcut { sequence: "Ctrl+R"; onActivated: photoService.search(searchField.text) }
    Shortcut {
        sequence: "Q"
        context: Qt.WindowShortcut
        enabled: !searchField.activeFocus && !serverField.activeFocus && !tokenField.activeFocus
        onActivated: Qt.quit()
    }
    Connections { target: photoService; function onConnectedChanged() { if (photoService.connected) root.showSettings = false } }
    Connections {
        target: photoService
        function onOriginalDownloaded(title, hash, folder, filePath, originalName) {
            root.showEditor = true
            imageEditor.openOriginal(filePath, title, hash, folder, originalName)
        }
        function onOriginalDownloadFailed(message) { root.saveNotice = message; noticeTimer.restart() }
    }
    Connections {
        target: imageEditor
        function onSaved(path) { root.saveNotice = "Export saved: " + path; noticeTimer.restart() }
    }
    Timer { id: noticeTimer; interval: 7000; onTriggered: root.saveNotice = "" }

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
                HeaderButton { text: "▦  LIBRARY"; themeColors: root.themeColors; accent: palette.pink; selected: !showSettings && photoService.selectedFolder === ""; onClicked: { showSettings = false; photoService.search(""); selectedPhoto = null } }
                HeaderButton { text: "⌘  SOURCE"; themeColors: root.themeColors; accent: palette.cyan; selected: false; onClicked: Qt.openUrlExternally("https://github.com/cybercore-tech/ghostframe") }
                HeaderButton { text: "⚙  SETTINGS"; themeColors: root.themeColors; accent: palette.lime; selected: showSettings; onClicked: showSettings = !showSettings }
                Button {
                    id: closeButton
                    text: "×"
                    Accessible.name: "Close Ghostframe"
                    Layout.preferredWidth: 42
                    Layout.preferredHeight: 42
                    onClicked: Qt.quit()
                    contentItem: Text { text: closeButton.text; color: palette.darkText; font.family: "Inter"; font.pixelSize: 25; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: closeButton.hovered ? Qt.lighter(palette.pink, 1.14) : palette.pink; border.color: palette.darkText; border.width: 1; radius: 1 }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            Rectangle {
                Layout.preferredWidth: 210; Layout.fillHeight: true; color: palette.panel
                Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: palette.line }
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 18; spacing: 15
                    Text { text: "◈  COLLECTIONS"; color: palette.text; font.family: "JetBrains Mono"; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1 }
                    SideItem { themeColors: root.themeColors; icon: "▦"; label: "All Images"; selected: !showSettings && photoService.selectedFolder === ""; accent: palette.cyan; onClicked: { showSettings = false; photoService.search(""); selectedPhoto = null } }
                    SideItem { themeColors: root.themeColors; icon: "◷"; label: "Recently Added"; selected: false; accent: palette.pink; onClicked: { showSettings = false; photoService.search("order:newest") } }
                    SideItem { themeColors: root.themeColors; icon: "☆"; label: "Favorites"; selected: false; accent: palette.lime; onClicked: { showSettings = false; photoService.search("favorite:true") } }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: palette.line; Layout.topMargin: 2; Layout.bottomMargin: 1 }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "▤  FOLDERS"; color: palette.lime; font.family: "JetBrains Mono"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1; Layout.fillWidth: true }
                        ToolButton {
                            id: folderRefresh
                            text: "↻"
                            onClicked: photoService.refreshFolders()
                            background: Rectangle {
                                color: folderRefresh.hovered ? palette.panel2 : "transparent"
                            }
                            contentItem: Text {
                                text: folderRefresh.text
                                color: palette.muted
                                font.pixelSize: 14
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                    ListView {
                        id: folderList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 90
                        model: photoService.folders
                        clip: true
                        spacing: 2
                        ScrollBar.vertical: ScrollBar { active: true }
                        delegate: Item {
                            required property var modelData
                            property var themeColors: root.themeColors
                            width: folderList.width
                            height: 28
                            property color folderAccent: modelData.depth % 3 === 0 ? themeColors.cyan : (modelData.depth % 3 === 1 ? themeColors.pink : themeColors.lime)
                            Rectangle { anchors.fill: parent; color: photoService.selectedFolder === modelData.path ? themeColors.panel2 : (folderHover.containsMouse ? themeColors.panelHover : "transparent"); border.color: photoService.selectedFolder === modelData.path ? folderAccent : (folderHover.containsMouse ? themeColors.line : "transparent"); border.width: 1; radius: 2 }
                            Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 2; color: folderAccent; visible: photoService.selectedFolder === modelData.path || folderHover.containsMouse }
                            Text { anchors.fill: parent; anchors.leftMargin: 10 + modelData.depth * 10; anchors.rightMargin: 8; verticalAlignment: Text.AlignVCenter; text: "›  " + modelData.title; color: photoService.selectedFolder === modelData.path ? folderAccent : (folderHover.containsMouse ? themeColors.text : themeColors.muted); font.family: "Inter"; font.pixelSize: 11; elide: Text.ElideMiddle }
                            MouseArea { id: folderHover; anchors.fill: parent; hoverEnabled: true; onClicked: { showSettings = false; selectedPhoto = null; photoService.browseFolder(modelData.path) } }
                        }
                    }
                    Text { text: photoService.connected ? photoService.photoCount + " indexed items" : "Connect PhotoPrism to browse"; color: palette.muted; font.family: "JetBrains Mono"; font.pixelSize: 9; wrapMode: Text.Wrap; Layout.fillWidth: true }
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
                        required property string hash
                        required property string folder
                        required property string originalName
                        width: grid.cellWidth - 12; height: grid.cellHeight - 12
                        Rectangle {
                            anchors.fill: parent
                            color: palette.panel
                            border.color: mouse.containsMouse ? palette.cyan : "#5b6875"
                            border.width: mouse.containsMouse ? 2 : 1
                            radius: 3
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 5
                                color: palette.bg
                                border.color: palette.line
                                border.width: 1
                                radius: 2
                                Image { id: thumb; anchors.fill: parent; anchors.margins: 1; source: thumbnail; fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: true
                                    Rectangle { anchors.fill: parent; color: palette.panel2; visible: thumb.status !== Image.Ready
                                        Text { anchors.centerIn: parent; text: thumb.status === Image.Error ? "IMAGE UNAVAILABLE" : "◌  LOADING"; color: palette.muted; font.family: "JetBrains Mono"; font.pixelSize: 9 }
                                    }
                                }
                                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 48; color: "#f20a0e13"; border.color: palette.line; border.width: 1 }
                                Rectangle { anchors.left: parent.left; anchors.bottom: parent.bottom; width: mouse.containsMouse ? 4 : 2; height: 48; color: mouse.containsMouse ? palette.pink : palette.cyan }
                                Column { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 9; spacing: 3
                                    Text { width: parent.width; text: title; color: palette.text; font.family: "Inter"; font.pixelSize: 11; elide: Text.ElideRight }
                                    Text { width: parent.width; text: subtitle; color: palette.cyan; font.family: "JetBrains Mono"; font.pixelSize: 9; elide: Text.ElideRight }
                                }
                            }
                            MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: root.selectedPhoto = ({title: title, subtitle: subtitle, source: fullImage, hash: hash, folder: folder, originalName: originalName}) }
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
                            TextField { id: tokenField; Layout.fillWidth: true; placeholderText: "Read-only app password · session only"; echoMode: TextInput.Password; color: palette.text; font.family: "JetBrains Mono"; background: Rectangle { color: palette.bg; border.color: palette.line; border.width: 1; radius: 2 } }
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
                    Button {
                        anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 20
                        text: imageEditor.busy ? "PREPARING…" : "✎  EDIT IMAGE"
                        enabled: !imageEditor.busy
                        onClicked: photoService.downloadOriginal(root.selectedPhoto.hash, root.selectedPhoto.title, root.selectedPhoto.folder, root.selectedPhoto.originalName)
                        contentItem: Text { text: parent.text; color: palette.darkText; font.family: "JetBrains Mono"; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: parent.hovered ? Qt.lighter(palette.cyan, 1.1) : palette.cyan; border.color: palette.darkText; radius: 2 }
                    }
                    Rectangle { anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 20; width: 42; height: 42; color: palette.panel; border.color: palette.pink
                        Text { anchors.centerIn: parent; text: "×"; color: palette.text; font.pixelSize: 25 }
                        MouseArea { anchors.fill: parent; onClicked: root.selectedPhoto = null }
                    }
                    Text { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 24; text: root.selectedPhoto ? root.selectedPhoto.title + "   //   " + root.selectedPhoto.subtitle : ""; color: palette.text; font.family: "JetBrains Mono"; font.pixelSize: 12 }
                }
                Rectangle {
                    anchors.fill: parent; visible: root.showEditor; z: 5; color: "#f0080a0d"
                    Rectangle {
                        anchors.fill: parent; anchors.margins: 24; color: palette.panel; border.color: palette.cyan; border.width: 1; radius: 3
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 18; spacing: 12
                            RowLayout {
                                Layout.fillWidth: true
                                ColumnLayout { Layout.fillWidth: true; spacing: 3
                                    Text { text: "GHOSTFRAME // IMAGE LAB"; color: palette.cyan; font.family: "JetBrains Mono"; font.pixelSize: 16; font.bold: true; font.letterSpacing: 1.2 }
                                    Text { text: imageEditor.sourceTitle + "  ·  Full-resolution nondestructive export"; color: palette.muted; font.family: "Inter"; font.pixelSize: 11; elide: Text.ElideRight }
                                }
                                Button { text: "×"; onClicked: { imageEditor.cancel(); root.showEditor = false }
                                    contentItem: Text { text: "×"; color: palette.darkText; font.pixelSize: 23; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: palette.pink; border.color: palette.darkText; radius: 2 }
                                }
                            }
                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: palette.line }
                            RowLayout {
                                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 16
                                Rectangle {
                                    Layout.fillWidth: true; Layout.fillHeight: true; color: "#07090c"; border.color: palette.line; radius: 2; clip: true
                                    Image {
                                        id: editPreview
                                        anchors.centerIn: parent
                                        width: imageEditor.displayWidth > 0 && imageEditor.displayHeight > 0 ? Math.min(parent.width, parent.height * imageEditor.displayWidth / imageEditor.displayHeight) : parent.width
                                        height: imageEditor.displayWidth > 0 && imageEditor.displayHeight > 0 ? Math.min(parent.height, parent.width * imageEditor.displayHeight / imageEditor.displayWidth) : parent.height
                                        source: imageEditor.previewUrl; fillMode: Image.PreserveAspectFit; asynchronous: true; cache: false
                                    }
                                    Rectangle {
                                        visible: imageEditor.hasCrop; x: editPreview.x + imageEditor.cropX * editPreview.width; y: editPreview.y + imageEditor.cropY * editPreview.height
                                        width: imageEditor.cropWidth * editPreview.width; height: imageEditor.cropHeight * editPreview.height
                                        color: "#1835f4ff"; border.color: palette.cyan; border.width: 2
                                    }
                                    MouseArea {
                                        anchors.fill: editPreview; enabled: editPreview.status === Image.Ready
                                        property point startPoint
                                        onPressed: startPoint = Qt.point(mouse.x, mouse.y)
                                        onReleased: {
                                            const left = Math.min(startPoint.x, mouse.x) / width
                                            const top = Math.min(startPoint.y, mouse.y) / height
                                            const cropWidth = Math.abs(mouse.x - startPoint.x) / width
                                            const cropHeight = Math.abs(mouse.y - startPoint.y) / height
                                            if (cropWidth > 0.015 && cropHeight > 0.015) imageEditor.setCrop(left, top, cropWidth, cropHeight)
                                        }
                                    }
                                    Text { anchors.centerIn: parent; visible: !imageEditor.ready && !imageEditor.busy; text: imageEditor.status; color: palette.muted; font.family: "JetBrains Mono"; font.pixelSize: 11; wrapMode: Text.Wrap; width: parent.width - 40; horizontalAlignment: Text.AlignHCenter }
                                }
                                ScrollView {
                                    Layout.preferredWidth: 330; Layout.fillHeight: true; clip: true
                                    ColumnLayout {
                                        width: 310; spacing: 10
                                        Text { text: "TRANSFORM"; color: palette.lime; font.family: "JetBrains Mono"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1 }
                                        RowLayout { Layout.fillWidth: true
                                            Button { text: "↶  ROTATE LEFT"; Layout.fillWidth: true; onClicked: imageEditor.rotateBy(-90); contentItem: Text { text: parent.text; color: palette.darkText; font.family: "JetBrains Mono"; font.pixelSize: 9; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter } background: Rectangle { color: palette.cyan; radius: 2 } }
                                            Button { text: "ROTATE RIGHT  ↷"; Layout.fillWidth: true; onClicked: imageEditor.rotateBy(90); contentItem: Text { text: parent.text; color: palette.darkText; font.family: "JetBrains Mono"; font.pixelSize: 9; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter } background: Rectangle { color: palette.cyan; radius: 2 } }
                                        }
                                        RowLayout { Layout.fillWidth: true
                                            Button { text: "CLEAR CROP"; Layout.fillWidth: true; onClicked: imageEditor.clearCrop(); contentItem: Text { text: parent.text; color: palette.text; font.family: "JetBrains Mono"; font.pixelSize: 9; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter } background: Rectangle { color: palette.panel2; border.color: palette.line; radius: 2 } }
                                            Button { text: "RESET ALL"; Layout.fillWidth: true; onClicked: imageEditor.resetEdits(); contentItem: Text { text: parent.text; color: palette.text; font.family: "JetBrains Mono"; font.pixelSize: 9; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter } background: Rectangle { color: palette.panel2; border.color: palette.line; radius: 2 } }
                                        }
                                        Text { text: "Drag over the preview to crop"; color: palette.muted; font.family: "Inter"; font.pixelSize: 10 }
                                        Text { text: "LIGHT  " + Math.round(imageEditor.brightness); color: palette.text; font.family: "JetBrains Mono"; font.pixelSize: 10 }
                                        Slider { id: lightSlider; Layout.fillWidth: true; from: -50; to: 50; value: imageEditor.brightness; onMoved: imageEditor.setAdjustments(value, contrastSlider.value, saturationSlider.value) }
                                        Text { text: "CONTRAST  " + Math.round(imageEditor.contrast); color: palette.text; font.family: "JetBrains Mono"; font.pixelSize: 10 }
                                        Slider { id: contrastSlider; Layout.fillWidth: true; from: -50; to: 50; value: imageEditor.contrast; onMoved: imageEditor.setAdjustments(lightSlider.value, value, saturationSlider.value) }
                                        Text { text: "SATURATION  " + Math.round(imageEditor.saturation) + "%"; color: palette.text; font.family: "JetBrains Mono"; font.pixelSize: 10 }
                                        Slider { id: saturationSlider; Layout.fillWidth: true; from: 0; to: 200; value: imageEditor.saturation; onMoved: imageEditor.setAdjustments(lightSlider.value, contrastSlider.value, value) }
                                        Text { text: "CYBERMETA"; color: palette.pink; font.family: "JetBrains Mono"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1; Layout.topMargin: 5 }
                                        TextField { id: metaTitle; Layout.fillWidth: true; text: imageEditor.sourceTitle; placeholderText: "Title"; color: palette.text; font.family: "Inter"; background: Rectangle { color: palette.bg; border.color: palette.line } }
                                        TextField { id: metaCaption; Layout.fillWidth: true; placeholderText: "Caption / description"; color: palette.text; font.family: "Inter"; background: Rectangle { color: palette.bg; border.color: palette.line } }
                                        TextField { id: metaTags; Layout.fillWidth: true; placeholderText: "Tags, separated by commas"; color: palette.text; font.family: "Inter"; background: Rectangle { color: palette.bg; border.color: palette.line } }
                                        Text { text: "Writes an XMP sidecar with source hash, folder, edit recipe, title, caption and tags."; color: palette.muted; font.family: "Inter"; font.pixelSize: 9; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                        Button {
                                            Layout.fillWidth: true; Layout.preferredHeight: 42; text: imageEditor.busy ? "WORKING…" : "SAVE EXPORT + CYBERMETA"
                                            enabled: imageEditor.ready && !imageEditor.busy
                                            onClicked: imageEditor.save(metaTitle.text, metaCaption.text, metaTags.text)
                                            contentItem: Text { text: parent.text; color: palette.darkText; font.family: "JetBrains Mono"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                            background: Rectangle { color: parent.enabled ? (parent.hovered ? Qt.lighter(palette.lime, 1.1) : palette.lime) : palette.line; radius: 2 }
                                        }
                                        Text { text: imageEditor.status; color: palette.cyan; font.family: "Inter"; font.pixelSize: 10; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 30; color: palette.panel
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: palette.line }
            RowLayout { anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 18
                Text { text: "◆  " + (root.saveNotice || photoService.status); color: root.saveNotice ? palette.cyan : palette.muted; font.family: "JetBrains Mono"; font.pixelSize: 9; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: "GHOSTFRAME  /  v0.1.0"; color: palette.lime; font.family: "JetBrains Mono"; font.pixelSize: 9; font.letterSpacing: 1 }
            }
        }
    }

    component HeaderButton: Button {
        id: headerButton
        property var themeColors
        property color accent: "#35f4ff"
        property bool selected: false
        contentItem: Text { text: headerButton.text; color: headerButton.themeColors.darkText; font.family: "JetBrains Mono"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 0.4; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { color: headerButton.hovered ? Qt.lighter(headerButton.accent, 1.14) : headerButton.accent; border.color: headerButton.selected ? headerButton.themeColors.text : headerButton.themeColors.darkText; border.width: headerButton.selected ? 2 : 1; radius: 1 }
    }
    component SideItem: Item {
        property var themeColors
        property string icon
        property string label
        property bool selected: false
        property color accent: palette.cyan
        signal clicked()
        Layout.fillWidth: true; Layout.preferredHeight: 32
        Rectangle { anchors.fill: parent; color: selected ? themeColors.panel2 : (itemHover.containsMouse ? themeColors.panelHover : "transparent"); border.color: selected ? accent : (itemHover.containsMouse ? themeColors.line : "transparent"); border.width: 1; radius: 2 }
        Row { anchors.fill: parent; anchors.leftMargin: 10; spacing: 10
            Text { text: icon; color: accent; font.family: "JetBrains Mono"; font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter }
            Text { text: label; color: selected ? themeColors.text : (itemHover.containsMouse ? accent : themeColors.muted); font.family: "Inter"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
        }
        MouseArea { id: itemHover; anchors.fill: parent; hoverEnabled: true; onClicked: parent.clicked() }
    }
}
