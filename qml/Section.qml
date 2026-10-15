import QtQuick
import QtQuick.Layouts

// Collapsible editor panel section with an optional reset button.
ColumnLayout {
    id: section
    property string title
    property color accent: Theme.lime
    property bool expanded: true
    // Recipe keys this section owns; RESET puts them back as one undo step.
    property var resetKeys: []
    default property alias content: body.data

    Layout.fillWidth: true
    spacing: 6

    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line; Layout.topMargin: 4 }
    RowLayout {
        Layout.fillWidth: true
        Text {
            text: (section.expanded ? "▾  " : "▸  ") + section.title
            color: section.accent; font.family: Theme.mono; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1
            Layout.fillWidth: true
            MouseArea { anchors.fill: parent; onClicked: section.expanded = !section.expanded }
        }
        Text {
            visible: section.resetKeys.length > 0 && section.expanded
            text: "RESET"; color: resetHover.containsMouse ? section.accent : Theme.muted
            font.family: Theme.mono; font.pixelSize: 9
            MouseArea { id: resetHover; anchors.fill: parent; hoverEnabled: true; enabled: imageEditor.ready
                onClicked: imageEditor.resetParameters(section.resetKeys, "Reset " + section.title.toLowerCase()) }
        }
    }
    ColumnLayout {
        id: body
        Layout.fillWidth: true
        visible: section.expanded
        spacing: 6
    }
}
