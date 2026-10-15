import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// 3D LUTs (.cube) from your LUT folder.
ColumnLayout {
    id: panel
    Layout.fillWidth: true
    spacing: 6

    ComboBox {
        id: lutBox
        Layout.fillWidth: true
        model: ["— NONE —"].concat(imageEditor.lutNames)
        currentIndex: Math.max(0, imageEditor.lutNames.indexOf(imageEditor.recipe.lutName || "") + 1)
        onActivated: (index) => imageEditor.applyParameter("lutName", index === 0 ? "" : imageEditor.lutNames[index - 1],
                                                          index === 0 ? "Remove LUT" : "LUT: " + imageEditor.lutNames[index - 1])
        enabled: imageEditor.ready
        font.family: Theme.mono; font.pixelSize: 10
        contentItem: Text { leftPadding: 8; text: lutBox.displayText; color: Theme.text; font: lutBox.font; verticalAlignment: Text.AlignVCenter; elide: Text.ElideMiddle }
        background: Rectangle { implicitHeight: 28; color: Theme.bg; border.color: Theme.line; radius: 2 }
    }
    RowLayout {
        Layout.fillWidth: true
        EditorButton { text: "IMPORT .CUBE"; primary: false; Layout.fillWidth: true; enabled: imageEditor.ready; onClicked: picker.open() }
        EditorButton { text: "OPEN FOLDER"; primary: false; Layout.fillWidth: true
                       onClicked: { imageEditor.refreshLuts(); Qt.openUrlExternally("file://" + imageEditor.lutDirectory) } }
    }
    ParamSlider { key: "lutAmount"; label: "LUT STRENGTH"; from: 0; to: 100; defaultValue: 100; suffix: "%" }
    FileDialog {
        id: picker
        title: "Import a 3D LUT"
        nameFilters: ["3D LUT (*.cube *.CUBE)"]
        onAccepted: imageEditor.importLut(selectedFile)
    }
}
