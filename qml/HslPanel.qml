import QtQuick
import QtQuick.Layouts

// HSL per colour band: pick a band, then shift its hue, saturation and
// luminance.
ColumnLayout {
    id: hsl
    property string band: "red"
    readonly property var bands: [
        ["red", "#ff4a4a"], ["orange", "#ff9a3c"], ["yellow", "#ffe14a"], ["green", "#5fdc5a"],
        ["aqua", "#35f4ff"], ["blue", "#4a7dff"], ["purple", "#a35cff"], ["magenta", "#ff36a8"]]
    readonly property color accent: bands.find(b => b[0] === band)[1]

    Layout.fillWidth: true
    spacing: 6

    RowLayout {
        Layout.fillWidth: true
        spacing: 4
        Repeater {
            model: hsl.bands
            delegate: Rectangle {
                required property var modelData
                readonly property string name: modelData[0]
                readonly property bool edited: ["hue", "saturation", "luminance"].some(k => Math.abs(imageEditor.recipe["hsl." + name + "." + k] || 0) > 1e-6)
                Layout.fillWidth: true; Layout.preferredHeight: 20; radius: 2
                color: modelData[1]
                opacity: hsl.band === name ? 1 : 0.45
                border.color: hsl.band === name ? Theme.text : "transparent"; border.width: 2
                Rectangle { visible: parent.edited; width: 5; height: 5; radius: 1; color: Theme.bg; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 3 }
                MouseArea { anchors.fill: parent; onClicked: hsl.band = parent.name }
            }
        }
    }
    ParamSlider { key: "hsl." + hsl.band + ".hue"; label: hsl.band.toUpperCase() + " HUE"; accent: hsl.accent; displayScale: 0.3; decimals: 0; suffix: "°" }
    ParamSlider { key: "hsl." + hsl.band + ".saturation"; label: hsl.band.toUpperCase() + " SATURATION"; accent: hsl.accent }
    ParamSlider { key: "hsl." + hsl.band + ".luminance"; label: hsl.band.toUpperCase() + " LUMINANCE"; accent: hsl.accent }
}
