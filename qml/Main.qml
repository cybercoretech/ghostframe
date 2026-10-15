import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Window shell: layout, app-level state and shortcuts. The pieces live in
// their own files (Header, Sidebar, PhotoGrid, Viewer, Editor, ...).
ApplicationWindow {
    id: root
    width: 1440
    height: 940
    minimumWidth: 900
    minimumHeight: 620
    visible: true
    title: "GHOSTFRAME // Cybercore Photo Archive"
    color: Theme.bg

    property var selectedPhoto: null
    // Driven by the Settings button and first-run setup, never by whether the
    // current folder happens to contain images.
    property bool showSettings: !photoService.connected && !photoService.startupConnecting
    property bool showEditor: false
    property string savedUrl: photoService.serverUrl !== "" ? photoService.serverUrl : "http://127.0.0.1:2342"
    property string saveNotice: ""
    // Single-key shortcuts must not fire while typing.
    readonly property bool typing: activeFocusItem instanceof TextInput

    function closeEditor() { imageEditor.cancel(); showEditor = false }
    function closeCurrentLayer() {
        if (showEditor) closeEditor()
        else if (showSettings) showSettings = false
        else if (selectedPhoto !== null) selectedPhoto = null
        else Qt.quit()
    }
    function showLibrary() { showSettings = false; selectedPhoto = null; photoService.search("") }
    function notify(message) { saveNotice = message; noticeTimer.restart() }

    Shortcut { sequence: "Ctrl+K"; onActivated: header.focusSearch() }
    Shortcut { sequence: "Ctrl+R"; onActivated: photoService.search(header.searchText) }
    Shortcut { sequence: "Escape"; onActivated: { if (root.showEditor) root.closeEditor(); else { root.selectedPhoto = null; root.showSettings = false } } }
    Shortcut { sequence: "Q"; enabled: !root.typing; onActivated: root.closeCurrentLayer() }
    Shortcut { sequences: [StandardKey.Undo]; enabled: root.showEditor && !root.typing; onActivated: imageEditor.undo() }
    Shortcut { sequences: [StandardKey.Redo, "Ctrl+Shift+Z"]; enabled: root.showEditor && !root.typing; onActivated: imageEditor.redo() }

    Connections {
        target: photoService
        function onConnectedChanged() { if (photoService.connected) root.showSettings = false }
        // A temporary server outage must not reopen the password form.
        function onStartupConnectingChanged() { if (photoService.startupConnecting) root.showSettings = false }
        function onOriginalDownloaded(title, hash, folder, filePath, originalName) {
            root.showEditor = true
            imageEditor.openOriginal(filePath, title, hash, folder, originalName)
        }
        function onOriginalDownloadFailed(message) { root.notify(message) }
    }
    Connections {
        target: imageEditor
        function onSaved(path) { root.notify("Export saved: " + path) }
    }
    Timer { id: noticeTimer; interval: 7000; onTriggered: root.saveNotice = "" }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TitleStrip { Layout.fillWidth: true; Layout.preferredHeight: 34 }

        Header {
            id: header
            Layout.fillWidth: true; Layout.preferredHeight: 72
            librarySelected: !root.showSettings && photoService.selectedFolder === ""
            settingsSelected: root.showSettings
            closeLabel: root.showSettings || root.showEditor || root.selectedPhoto !== null ? "Close current panel" : "Close Ghostframe"
            onSearchRequested: (query) => photoService.search(query)
            onLibraryClicked: root.showLibrary()
            onSettingsClicked: root.showSettings = !root.showSettings
            onCloseClicked: root.closeCurrentLayer()
        }

        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0

            Sidebar {
                Layout.preferredWidth: 210; Layout.fillHeight: true
                allImagesSelected: !root.showSettings && photoService.selectedFolder === ""
                onAllImagesClicked: root.showLibrary()
                onSearchClicked: (query) => { root.showSettings = false; photoService.search(query) }
                onFolderClicked: (path) => { root.showSettings = false; root.selectedPhoto = null; photoService.browseFolder(path) }
            }

            Item {
                Layout.fillWidth: true; Layout.fillHeight: true

                PhotoGrid {
                    anchors.fill: parent; anchors.margins: 20
                    onPhotoOpened: (photo) => root.selectedPhoto = photo
                }
                ConnectionPanel {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 50, 500)
                    visible: root.showSettings || photoService.photoCount === 0
                    showForm: root.showSettings
                    savedUrl: root.savedUrl
                    onConnectRequested: (url, token) => { root.savedUrl = url; photoService.connectToServer(url, token) }
                    onCloseRequested: root.showSettings = false
                }
                Viewer {
                    anchors.fill: parent; z: 3
                    photo: root.selectedPhoto
                    onCloseRequested: root.selectedPhoto = null
                    onEditRequested: photoService.downloadOriginal(root.selectedPhoto.hash, root.selectedPhoto.title, root.selectedPhoto.folder, root.selectedPhoto.originalName)
                }
                Editor {
                    anchors.fill: parent; z: 5
                    visible: root.showEditor
                    onCloseRequested: root.closeEditor()
                }
            }
        }

        StatusBar { Layout.fillWidth: true; Layout.preferredHeight: 30; notice: root.saveNotice }
    }
}
