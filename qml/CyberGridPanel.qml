import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// CYBERGRID gradient maps: pick a Cybercore palette and a style (neon,
// sunset, duotones…); image brightness is mapped onto those colours.
ColumnLayout {
    id: panel
    property string themeId: cybergrid.activeId !== "" ? cybergrid.activeId : "cyberdyne-hunter"
    property string styleId: "neon"
    readonly property var preview: cybergrid.gradient(themeId, styleId)

    function apply() {
        imageEditor.setParameter("gradientColors", preview)
        imageEditor.setParameter("gradientName", themeId + " · " + styleId)
        if ((imageEditor.recipe.gradientAmount || 0) === 0) imageEditor.setParameter("gradientAmount", 60)
        imageEditor.commitEdit("Gradient map: " + themeId)
    }

    Layout.fillWidth: true
    spacing: 6

    ComboBox {
        id: themeBox
        Layout.fillWidth: true
        model: cybergrid.themes
        textRole: "id"
        currentIndex: Math.max(0, cybergrid.themes.findIndex(t => t.id === panel.themeId))
        onActivated: (index) => panel.themeId = cybergrid.themes[index].id
        font.family: Theme.mono; font.pixelSize: 10
        contentItem: Text { leftPadding: 8; text: themeBox.displayText.toUpperCase(); color: Theme.text; font: themeBox.font; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
        background: Rectangle { implicitHeight: 28; color: Theme.bg; border.color: themeBox.activeFocus ? Theme.pink : Theme.line; radius: 2 }
    }
    Flow {
        Layout.fillWidth: true; spacing: 4
        Repeater {
            model: cybergrid.styles
            delegate: Chip {
                required property var modelData
                text: modelData.label; accent: Theme.pink
                selected: panel.styleId === modelData.id
                onClicked: panel.styleId = modelData.id
            }
        }
    }
    // The gradient itself: one two-stop segment between each pair of colours.
    Row {
        id: swatch
        Layout.fillWidth: true; Layout.preferredHeight: 18
        Repeater {
            model: Math.max(0, panel.preview.length - 1)
            delegate: Rectangle {
                required property int index
                width: swatch.width / Math.max(1, panel.preview.length - 1); height: swatch.height
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: "#" + panel.preview[index] }
                    GradientStop { position: 1; color: "#" + panel.preview[index + 1] }
                }
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        EditorButton { text: "APPLY GRADIENT"; accent: Theme.pink; Layout.fillWidth: true; enabled: imageEditor.ready && panel.preview.length >= 2; onClicked: panel.apply() }
        EditorButton { text: "REMOVE"; primary: false; enabled: imageEditor.ready && (imageEditor.recipe.gradientColors || []).length > 0
                       onClicked: imageEditor.resetParameters(["gradientColors", "gradientName", "gradientAmount"], "Remove gradient map") }
    }
    ParamSlider { key: "gradientAmount"; label: "GRADIENT AMOUNT"; from: 0; to: 100; accent: Theme.pink; suffix: "%" }
    Text {
        visible: (imageEditor.recipe.gradientName || "") !== ""
        text: "Applied: " + (imageEditor.recipe.gradientName || "").toUpperCase()
        color: Theme.muted; font.family: Theme.mono; font.pixelSize: 9
    }
}
