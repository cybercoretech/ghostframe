import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Wordmark, search and the main navigation buttons.
Rectangle {
    id: header
    property bool librarySelected: false
    property bool settingsSelected: false
    property string closeLabel: "Close Ghostframe"
    readonly property alias searchText: searchField.text
    readonly property bool searchFocused: searchField.activeFocus
    signal searchRequested(string query)
    signal libraryClicked()
    signal settingsClicked()
    signal closeClicked()

    function focusSearch() { searchField.forceActiveFocus() }

    color: Theme.bg
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
    RowLayout {
        anchors.fill: parent; anchors.leftMargin: 22; anchors.rightMargin: 22; spacing: 9
        Column {
            Layout.preferredWidth: 265; spacing: 0
            Text { text: "GHOSTFRAME"; color: Theme.text; font.family: Theme.mono; font.pixelSize: 26; font.bold: true; font.letterSpacing: 1 }
            Text { text: "CYBERCORE PHOTO SYSTEMS"; color: Theme.lime; font.family: Theme.mono; font.pixelSize: 9; font.letterSpacing: 2 }
        }
        Item { Layout.fillWidth: true }
        TextField {
            id: searchField
            Layout.preferredWidth: 250; Layout.preferredHeight: 42
            placeholderText: "⌕  Search your archive  Ctrl+K"
            color: Theme.text; placeholderTextColor: Theme.muted
            font.family: Theme.mono; font.pixelSize: 12
            background: Rectangle { color: Theme.panel; border.color: Theme.cyan; border.width: searchField.activeFocus ? 2 : 1; radius: 2 }
            onAccepted: header.searchRequested(text)
        }
        HeaderButton { text: "▦  LIBRARY"; accent: Theme.pink; selected: header.librarySelected; onClicked: header.libraryClicked() }
        HeaderButton { text: "⌘  SOURCE"; accent: Theme.cyan; onClicked: Qt.openUrlExternally("https://github.com/cybercore-tech/ghostframe") }
        HeaderButton { text: "⚙  SETTINGS"; accent: Theme.lime; selected: header.settingsSelected; onClicked: header.settingsClicked() }
        Button {
            id: closeButton
            text: "×"
            Layout.preferredWidth: 42; Layout.preferredHeight: 42
            Accessible.name: header.closeLabel
            onClicked: header.closeClicked()
            contentItem: Text { text: closeButton.text; color: Theme.darkText; font.family: Theme.sans; font.pixelSize: 25; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            background: Rectangle { color: closeButton.hovered ? Qt.lighter(Theme.pink, 1.14) : Theme.pink; border.color: Theme.darkText; border.width: 1; radius: 1 }
        }
    }
}
