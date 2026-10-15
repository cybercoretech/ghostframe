import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Centre message for an empty view, plus the PhotoPrism connection form.
Column {
    id: panel
    property bool showForm: false
    property string savedUrl: "http://127.0.0.1:2342"
    signal connectRequested(string url, string token)
    signal closeRequested()

    spacing: 12
    Text {
        text: panel.showForm ? "◈  CONNECT YOUR ARCHIVE" : "⌁  NO IMAGES IN VIEW"
        color: Theme.lime; font.family: Theme.mono; font.pixelSize: 17; font.bold: true
        anchors.horizontalCenter: parent.horizontalCenter
    }
    Text {
        text: panel.showForm ? "Ghostframe is your custom local browser. Connect it to a PhotoPrism instance to see your images." : photoService.status
        color: Theme.muted; font.family: Theme.sans; font.pixelSize: 13; wrapMode: Text.Wrap; width: parent.width; horizontalAlignment: Text.AlignHCenter
    }
    Rectangle {
        visible: panel.showForm
        width: parent.width; height: 258; color: Theme.panel; border.color: Theme.line; radius: 2
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 18; spacing: 10
            RowLayout {
                Layout.fillWidth: true
                Text { text: "CONNECT YOUR ARCHIVE"; color: Theme.lime; font.family: Theme.mono; font.pixelSize: 11; font.bold: true; Layout.fillWidth: true }
                Button {
                    text: "×"; Accessible.name: "Close settings"
                    Layout.preferredWidth: 30; Layout.preferredHeight: 28
                    onClicked: panel.closeRequested()
                    contentItem: Text { text: "×"; color: Theme.darkText; font.pixelSize: 20; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: Theme.pink; border.color: Theme.darkText; radius: 2 }
                }
            }
            Text { text: "PHOTOPRISM ENDPOINT"; color: Theme.cyan; font.family: Theme.mono; font.pixelSize: 10; font.letterSpacing: 1 }
            TextField {
                id: serverField
                Layout.fillWidth: true; text: panel.savedUrl; placeholderText: "http://127.0.0.1:2342"
                color: Theme.text; font.family: Theme.mono
                background: Rectangle { color: Theme.bg; border.color: serverField.activeFocus ? Theme.cyan : Theme.line; border.width: 1; radius: 2 }
            }
            Text { text: "APP PASSWORD / SCOPED API TOKEN"; color: Theme.cyan; font.family: Theme.mono; font.pixelSize: 10; font.letterSpacing: 1 }
            TextField {
                id: tokenField
                Layout.fillWidth: true; placeholderText: "PhotoPrism app password · saved on this machine"; echoMode: TextInput.Password
                color: Theme.text; font.family: Theme.mono
                background: Rectangle { color: Theme.bg; border.color: tokenField.activeFocus ? Theme.cyan : Theme.line; border.width: 1; radius: 2 }
                onAccepted: connectButton.clicked()
            }
            RowLayout {
                Layout.fillWidth: true
                Text { Layout.fillWidth: true; text: photoService.status; color: Theme.muted; font.family: Theme.sans; font.pixelSize: 10; wrapMode: Text.Wrap }
                Button {
                    id: connectButton
                    text: "CONNECT  →"
                    onClicked: { panel.connectRequested(serverField.text, tokenField.text); tokenField.clear() }
                    contentItem: Text { text: connectButton.text; color: Theme.bg; font.family: Theme.mono; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: Theme.lime; radius: 2 }
                }
            }
        }
    }
}
