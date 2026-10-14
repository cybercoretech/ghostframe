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
    color: theme.bg

    QtObject {
        id: theme
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

    property var themeColors: theme
    property var selectedPhoto: null
    property bool showSettings: !photoService.connected && !photoService.startupConnecting
    // This panel is controlled by the Settings button (or initial connection
    // setup), never by whether the current folder happens to contain images.
    readonly property bool showConnectionPanel: showSettings
    property bool showEditor: false
    property string savedUrl: photoService.serverUrl !== "" ? photoService.serverUrl : "http://127.0.0.1:2342"
    property string saveNotice: ""

    function closeCurrentLayer() {
        if (showEditor) { imageEditor.cancel(); showEditor = false }
        else if (showSettings) showSettings = false
        else if (selectedPhoto !== null) selectedPhoto = null
        else Qt.quit()
    }

    Shortcut { sequence: "Ctrl+K"; onActivated: searchField.forceActiveFocus() }
    Shortcut { sequence: "Escape"; onActivated: { if (showEditor) { imageEditor.cancel(); showEditor = false } else { selectedPhoto = null; showSettings = false } } }
    Shortcut { sequence: "Ctrl+R"; onActivated: photoService.search(searchField.text) }
    Shortcut {
        sequence: "Q"
        context: Qt.WindowShortcut
        enabled: !searchField.activeFocus && !serverField.activeFocus && !tokenField.activeFocus && !metaTitle.activeFocus && !metaCaption.activeFocus && !metaTags.activeFocus
        onActivated: root.closeCurrentLayer()
    }
    Connections {
        target: photoService
        function onConnectedChanged() { if (photoService.connected) root.showSettings = false }
        function onStartupConnectingChanged() {
            if (photoService.startupConnecting) root.showSettings = false
            // A temporary server outage must not reopen the password form.
        }
    }
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
            color: theme.bg
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.line }
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 22; anchors.rightMargin: 22
                Text { text: "▮  GHOSTFRAME // LOCAL PHOTO ARCHIVE"; color: theme.lime; font.family: "JetBrains Mono"; font.pixelSize: 10; font.letterSpacing: 1.5 }
                Item { Layout.fillWidth: true }
                Text { text: "PHOTO INDEX  ·  " + (photoService.connected ? "ONLINE" : "OFFLINE"); color: photoService.connected ? theme.cyan : theme.muted; font.family: "JetBrains Mono"; font.pixelSize: 10; font.letterSpacing: 1.2 }
                Text { text: "   NODE 01"; color: theme.pink; font.family: "JetBrains Mono"; font.pixelSize: 10; font.letterSpacing: 1.2 }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 72
            color: theme.bg
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.line }
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 22; anchors.rightMargin: 22; spacing: 9
                Column {
                    Layout.preferredWidth: 265; spacing: 0
                    Text { text: "GHOSTFRAME"; color: theme.text; font.family: "JetBrains Mono"; font.pixelSize: 26; font.bold: true; font.letterSpacing: 1 }
                    Text { text: "CYBERCORE PHOTO SYSTEMS"; color: theme.lime; font.family: "JetBrains Mono"; font.pixelSize: 9; font.letterSpacing: 2 }
                }
                Item { Layout.fillWidth: true }
                TextField {
                    id: searchField
                    Layout.preferredWidth: 250
                    Layout.preferredHeight: 42
                    placeholderText: "⌕  Search your archive  Ctrl+K"
                    color: theme.text; placeholderTextColor: theme.muted
                    font.family: "JetBrains Mono"; font.pixelSize: 12
                    background: Rectangle { color: theme.panel; border.color: searchField.activeFocus ? theme.cyan : theme.cyan; border.width: 1; radius: 2 }
                    onAccepted: photoService.search(text)
                }
                HeaderButton { text: "▦  LIBRARY"; themeColors: root.themeColors; accent: theme.pink; selected: !showSettings && photoService.selectedFolder === ""; onClicked: { showSettings = false; photoService.search(""); selectedPhoto = null } }
                HeaderButton { text: "⌘  SOURCE"; themeColors: root.themeColors; accent: theme.cyan; selected: false; onClicked: Qt.openUrlExternally("https://github.com/cybercore-tech/ghostframe") }
                HeaderButton { text: "⚙  SETTINGS"; themeColors: root.themeColors; accent: theme.lime; selected: showSettings; onClicked: showSettings = !showSettings }
                Button {
                    id: closeButton
                    text: "×"
                    Layout.preferredWidth: 42
                    Layout.preferredHeight: 42
                    Accessible.name: showSettings || showEditor || selectedPhoto !== null ? "Close current panel" : "Close Ghostframe"
                    onClicked: root.closeCurrentLayer()
                    contentItem: Text { text: closeButton.text; color: theme.darkText; font.family: "Inter"; font.pixelSize: 25; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: closeButton.hovered ? Qt.lighter(theme.pink, 1.14) : theme.pink; border.color: theme.darkText; border.width: 1; radius: 1 }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            Rectangle {
                Layout.preferredWidth: 210; Layout.fillHeight: true; color: theme.panel
                Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: theme.line }
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 18; spacing: 15
                    Text { text: "◈  COLLECTIONS"; color: theme.text; font.family: "JetBrains Mono"; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1 }
                    SideItem { themeColors: root.themeColors; icon: "▦"; label: "All Images"; selected: !showSettings && photoService.selectedFolder === ""; accent: theme.cyan; onClicked: { showSettings = false; photoService.search(""); selectedPhoto = null } }
                    SideItem { themeColors: root.themeColors; icon: "◷"; label: "Recently Added"; selected: false; accent: theme.pink; onClicked: { showSettings = false; photoService.search("order:newest") } }
                    SideItem { themeColors: root.themeColors; icon: "☆"; label: "Favorites"; selected: false; accent: theme.lime; onClicked: { showSettings = false; photoService.search("favorite:true") } }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: theme.line; Layout.topMargin: 2; Layout.bottomMargin: 1 }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "▤  FOLDERS"; color: theme.lime; font.family: "JetBrains Mono"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1; Layout.fillWidth: true }
                        ToolButton {
                            id: folderRefresh
                            text: "↻"
                            onClicked: photoService.refreshFolders()
                            background: Rectangle {
                                color: folderRefresh.hovered ? theme.panel2 : "transparent"
                            }
                            contentItem: Text {
                                text: folderRefresh.text
                                color: theme.muted
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
                            required property int index
                            property var themeColors: root.themeColors
                            width: folderList.width
                            height: 30
                            property bool currentFolder: photoService.selectedFolder === modelData.path
                            property color folderAccent: index % 3 === 0 ? themeColors.cyan : (index % 3 === 1 ? themeColors.pink : themeColors.lime)
                            Rectangle {
                                anchors.fill: parent
                                color: currentFolder ? Qt.rgba(folderAccent.r, folderAccent.g, folderAccent.b, 0.22)
                                                     : folderHover.containsMouse ? Qt.rgba(folderAccent.r, folderAccent.g, folderAccent.b, 0.30) : "#10161d"
                                border.color: currentFolder || folderHover.containsMouse ? folderAccent : themeColors.line
                                border.width: currentFolder || folderHover.containsMouse ? 2 : 0
                                radius: 2
                            }
                            Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: folderHover.containsMouse || currentFolder ? 4 : 3; color: folderAccent; opacity: 1 }
                            Text {
                                anchors.fill: parent; anchors.leftMargin: 12 + modelData.depth * 10; anchors.rightMargin: 8
                                verticalAlignment: Text.AlignVCenter
                                text: "▸  " + modelData.title
                                color: currentFolder ? themeColors.text : folderAccent
                                font.family: "Inter"; font.pixelSize: 11; elide: Text.ElideMiddle
                            }
                            MouseArea { id: folderHover; anchors.fill: parent; hoverEnabled: true; onClicked: { showSettings = false; selectedPhoto = null; photoService.browseFolder(modelData.path) } }
                        }
                    }
                    Text { text: photoService.connected ? photoService.photoCount + " indexed items" : "Connect PhotoPrism to browse"; color: theme.muted; font.family: "JetBrains Mono"; font.pixelSize: 9; wrapMode: Text.Wrap; Layout.fillWidth: true }
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
                            color: theme.panel
                            // Cyan frame at rest, pink when hovered — never a plain white box.
                            border.color: mouse.containsMouse ? theme.pink : Qt.rgba(theme.cyan.r, theme.cyan.g, theme.cyan.b, 0.55)
                            border.width: mouse.containsMouse ? 2 : 1
                            radius: 2
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 3
                                color: theme.bg
                                radius: 2
                                clip: true
                                Image { id: thumb; anchors.fill: parent; anchors.margins: 1; source: thumbnail; fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: true
                                    Rectangle { anchors.fill: parent; color: theme.panel2; visible: thumb.status !== Image.Ready
                                        Text { anchors.centerIn: parent; text: thumb.status === Image.Error ? "IMAGE UNAVAILABLE" : "◌  LOADING"; color: theme.muted; font.family: "JetBrains Mono"; font.pixelSize: 9 }
                                    }
                                }
                                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 48; color: "#f20a0e13" }
                                Rectangle { anchors.left: parent.left; anchors.bottom: parent.bottom; width: mouse.containsMouse ? 4 : 2; height: 48; color: mouse.containsMouse ? theme.pink : theme.cyan }
                                Column { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 9; spacing: 3
                                    Text { width: parent.width; text: title; color: theme.text; font.family: "Inter"; font.pixelSize: 11; elide: Text.ElideRight }
                                    Text { width: parent.width; text: subtitle; color: theme.cyan; font.family: "JetBrains Mono"; font.pixelSize: 9; elide: Text.ElideRight }
                                }
                            }
                            MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: root.selectedPhoto = ({title: title, subtitle: subtitle, source: fullImage, hash: hash, folder: folder, originalName: originalName}) }
                        }
                    }
                    onAtYEndChanged: if (atYEnd) photoService.loadMore()
                    ScrollBar.vertical: ScrollBar { active: true }
                }
                Column {
                    anchors.centerIn: parent; width: Math.min(parent.width - 50, 500); spacing: 12; visible: showConnectionPanel || photoService.photoCount === 0
                    Text { text: showConnectionPanel ? "◈  CONNECT YOUR ARCHIVE" : "⌁  NO IMAGES IN VIEW"; color: theme.lime; font.family: "JetBrains Mono"; font.pixelSize: 17; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }
                    Text { text: showConnectionPanel ? "Ghostframe is your custom local browser. Connect it to a PhotoPrism instance to see your images." : photoService.status; color: theme.muted; font.family: "Inter"; font.pixelSize: 13; wrapMode: Text.Wrap; width: parent.width; horizontalAlignment: Text.AlignHCenter }
                    Rectangle {
                        visible: showConnectionPanel
                        width: parent.width; height: 258; color: theme.panel; border.color: theme.line; radius: 2
                        ColumnLayout { anchors.fill: parent; anchors.margins: 18; spacing: 10
                            RowLayout { Layout.fillWidth: true
                                Text { text: "CONNECT YOUR ARCHIVE"; color: theme.lime; font.family: "JetBrains Mono"; font.pixelSize: 11; font.bold: true; Layout.fillWidth: true }
                                Button { text: "×"; Accessible.name: "Close settings"; Layout.preferredWidth: 30; Layout.preferredHeight: 28; onClicked: root.showSettings = false
                                    contentItem: Text { text: "×"; color: theme.darkText; font.pixelSize: 20; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: theme.pink; border.color: theme.darkText; radius: 2 }
                                }
                            }
                            Text { text: "PHOTOPRISM ENDPOINT"; color: theme.cyan; font.family: "JetBrains Mono"; font.pixelSize: 10; font.letterSpacing: 1 }
                            TextField { id: serverField; Layout.fillWidth: true; text: root.savedUrl; placeholderText: "http://127.0.0.1:2342"; color: theme.text; font.family: "JetBrains Mono"; background: Rectangle { color: theme.bg; border.color: theme.line; border.width: 1; radius: 2 } }
                            Text { text: "APP PASSWORD / SCOPED API TOKEN"; color: theme.cyan; font.family: "JetBrains Mono"; font.pixelSize: 10; font.letterSpacing: 1 }
                            TextField { id: tokenField; Layout.fillWidth: true; placeholderText: "PhotoPrism app password · saved on this machine"; echoMode: TextInput.Password; color: theme.text; font.family: "JetBrains Mono"; background: Rectangle { color: theme.bg; border.color: theme.line; border.width: 1; radius: 2 } }
                            RowLayout { Layout.fillWidth: true
                                Text { Layout.fillWidth: true; text: photoService.status; color: theme.muted; font.family: "Inter"; font.pixelSize: 10; wrapMode: Text.Wrap }
                                Button { text: "CONNECT  →"; onClicked: { root.savedUrl = serverField.text; photoService.connectToServer(serverField.text, tokenField.text); tokenField.clear() }
                                    contentItem: Text { text: parent.text; color: theme.bg; font.family: "JetBrains Mono"; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: theme.lime; radius: 2 }
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
                        contentItem: Text { text: parent.text; color: theme.darkText; font.family: "JetBrains Mono"; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: parent.hovered ? Qt.lighter(theme.cyan, 1.1) : theme.cyan; border.color: theme.darkText; radius: 2 }
                    }
                    Rectangle { anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 20; width: 42; height: 42; color: theme.panel; border.color: theme.pink
                        Text { anchors.centerIn: parent; text: "×"; color: theme.text; font.pixelSize: 25 }
                        MouseArea { anchors.fill: parent; onClicked: root.selectedPhoto = null }
                    }
                    Text { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 24; text: root.selectedPhoto ? root.selectedPhoto.title + "   //   " + root.selectedPhoto.subtitle : ""; color: theme.text; font.family: "JetBrains Mono"; font.pixelSize: 12 }
                }
                Rectangle {
                    anchors.fill: parent; visible: root.showEditor; z: 5; color: "#f0080a0d"
                    Rectangle {
                        anchors.fill: parent; anchors.margins: 24; color: theme.panel; border.color: theme.cyan; border.width: 1; radius: 3
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 18; spacing: 12
                            RowLayout {
                                Layout.fillWidth: true
                                ColumnLayout { Layout.fillWidth: true; spacing: 3
                                    Text { text: "GHOSTFRAME // IMAGE LAB"; color: theme.cyan; font.family: "JetBrains Mono"; font.pixelSize: 16; font.bold: true; font.letterSpacing: 1.2 }
                                    Text { text: imageEditor.sourceTitle + "  ·  Full-resolution nondestructive export"; color: theme.muted; font.family: "Inter"; font.pixelSize: 11; elide: Text.ElideRight }
                                }
                                Button { text: "×"; onClicked: { imageEditor.cancel(); root.showEditor = false }
                                    contentItem: Text { text: "×"; color: theme.darkText; font.pixelSize: 23; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: theme.pink; border.color: theme.darkText; radius: 2 }
                                }
                            }
                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: theme.line }
                            RowLayout {
                                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 16
                                Rectangle {
                                    Layout.fillWidth: true; Layout.fillHeight: true; color: "#07090c"; border.color: theme.line; radius: 2; clip: true
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
                                        color: "#1835f4ff"; border.color: theme.cyan; border.width: 2
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
                                    Text { anchors.centerIn: parent; visible: editPreview.status !== Image.Ready; text: imageEditor.status; color: theme.muted; font.family: "JetBrains Mono"; font.pixelSize: 11; wrapMode: Text.Wrap; width: parent.width - 40; horizontalAlignment: Text.AlignHCenter }
                                }
                                ScrollView {
                                    Layout.preferredWidth: 330; Layout.fillHeight: true; clip: true
                                    ColumnLayout {
                                        width: 310; spacing: 10
                                        Text { text: "TRANSFORM"; color: theme.lime; font.family: "JetBrains Mono"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1 }
                                        RowLayout { Layout.fillWidth: true
                                            Button { text: "↶  ROTATE LEFT"; Layout.fillWidth: true; onClicked: imageEditor.rotateBy(-90); contentItem: Text { text: parent.text; color: theme.darkText; font.family: "JetBrains Mono"; font.pixelSize: 9; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter } background: Rectangle { color: theme.cyan; radius: 2 } }
                                            Button { text: "ROTATE RIGHT  ↷"; Layout.fillWidth: true; onClicked: imageEditor.rotateBy(90); contentItem: Text { text: parent.text; color: theme.darkText; font.family: "JetBrains Mono"; font.pixelSize: 9; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter } background: Rectangle { color: theme.cyan; radius: 2 } }
                                        }
                                        RowLayout { Layout.fillWidth: true
                                            Button { text: "CLEAR CROP"; Layout.fillWidth: true; onClicked: imageEditor.clearCrop(); contentItem: Text { text: parent.text; color: theme.text; font.family: "JetBrains Mono"; font.pixelSize: 9; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter } background: Rectangle { color: theme.panel2; border.color: theme.line; radius: 2 } }
                                            Button { text: "RESET ALL"; Layout.fillWidth: true; onClicked: imageEditor.resetEdits(); contentItem: Text { text: parent.text; color: theme.text; font.family: "JetBrains Mono"; font.pixelSize: 9; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter } background: Rectangle { color: theme.panel2; border.color: theme.line; radius: 2 } }
                                        }
                                        Text { text: "Drag over the preview to crop"; color: theme.muted; font.family: "Inter"; font.pixelSize: 10 }
                                        Text { text: "LIGHT  " + Math.round(imageEditor.brightness); color: theme.text; font.family: "JetBrains Mono"; font.pixelSize: 10 }
                                        Slider { id: lightSlider; Layout.fillWidth: true; from: -50; to: 50; value: imageEditor.brightness; onMoved: imageEditor.setAdjustments(value, contrastSlider.value, saturationSlider.value) }
                                        Text { text: "CONTRAST  " + Math.round(imageEditor.contrast); color: theme.text; font.family: "JetBrains Mono"; font.pixelSize: 10 }
                                        Slider { id: contrastSlider; Layout.fillWidth: true; from: -50; to: 50; value: imageEditor.contrast; onMoved: imageEditor.setAdjustments(lightSlider.value, value, saturationSlider.value) }
                                        Text { text: "SATURATION  " + Math.round(imageEditor.saturation) + "%"; color: theme.text; font.family: "JetBrains Mono"; font.pixelSize: 10 }
                                        Slider { id: saturationSlider; Layout.fillWidth: true; from: 0; to: 200; value: imageEditor.saturation; onMoved: imageEditor.setAdjustments(lightSlider.value, contrastSlider.value, value) }
                                        Text { text: "CYBERMETA"; color: theme.pink; font.family: "JetBrains Mono"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1; Layout.topMargin: 5 }
                                        TextField { id: metaTitle; Layout.fillWidth: true; text: imageEditor.sourceTitle; placeholderText: "Title"; color: theme.text; font.family: "Inter"; background: Rectangle { color: theme.bg; border.color: theme.line } }
                                        TextField { id: metaCaption; Layout.fillWidth: true; placeholderText: "Caption / description"; color: theme.text; font.family: "Inter"; background: Rectangle { color: theme.bg; border.color: theme.line } }
                                        TextField { id: metaTags; Layout.fillWidth: true; placeholderText: "Tags, separated by commas"; color: theme.text; font.family: "Inter"; background: Rectangle { color: theme.bg; border.color: theme.line } }
                                        Text { text: "Writes an XMP sidecar with source hash, folder, edit recipe, title, caption and tags."; color: theme.muted; font.family: "Inter"; font.pixelSize: 9; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                        Button {
                                            Layout.fillWidth: true; Layout.preferredHeight: 42; text: imageEditor.busy ? "WORKING…" : "SAVE EXPORT + CYBERMETA"
                                            enabled: imageEditor.ready && !imageEditor.busy
                                            onClicked: imageEditor.save(metaTitle.text, metaCaption.text, metaTags.text)
                                            contentItem: Text { text: parent.text; color: theme.darkText; font.family: "JetBrains Mono"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                            background: Rectangle { color: parent.enabled ? (parent.hovered ? Qt.lighter(theme.lime, 1.1) : theme.lime) : theme.line; radius: 2 }
                                        }
                                        Text { text: imageEditor.status; color: theme.cyan; font.family: "Inter"; font.pixelSize: 10; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 30; color: theme.panel
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: theme.line }
            RowLayout { anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 18
                Text { text: "◆  " + (root.saveNotice || photoService.status); color: root.saveNotice ? theme.cyan : theme.muted; font.family: "JetBrains Mono"; font.pixelSize: 9; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: "GHOSTFRAME  /  v0.1.0"; color: theme.lime; font.family: "JetBrains Mono"; font.pixelSize: 9; font.letterSpacing: 1 }
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
        property color accent: theme.cyan
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
