import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Looks: one click applies a whole style (adjustments + FX) and keeps this
// photo's crop, geometry and text. Save the current style as your own.
ColumnLayout {
    Layout.fillWidth: true
    spacing: 6
    Flow {
        Layout.fillWidth: true
        spacing: 4
        Repeater {
            model: imageEditor.looks
            delegate: Row {
                required property var modelData
                spacing: 0
                Chip {
                    text: parent.modelData.name.toUpperCase()
                    accent: parent.modelData.builtin ? Theme.cyan : Theme.lime
                    enabled: imageEditor.ready
                    onClicked: imageEditor.applyLook(parent.modelData.name)
                    ToolTip.visible: hovered; ToolTip.text: parent.modelData.builtin ? "Built-in look" : "Your look"
                }
                Chip {
                    visible: !parent.modelData.builtin
                    text: "×"; implicitWidth: 22; accent: Theme.pink
                    onClicked: imageEditor.deleteLook(parent.modelData.name)
                    ToolTip.visible: hovered; ToolTip.text: "Delete this look"
                }
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        MetaField { id: lookName; Layout.fillWidth: true; placeholderText: "Name for the current look" }
        Chip { text: "SAVE LOOK"; accent: Theme.lime; enabled: imageEditor.ready && lookName.text.trim() !== ""
               onClicked: { if (imageEditor.saveLook(lookName.text)) lookName.clear() } }
    }
    Text { text: "Looks keep this photo's crop, geometry and text."; color: Theme.muted; font.family: Theme.sans; font.pixelSize: 9 }
}
