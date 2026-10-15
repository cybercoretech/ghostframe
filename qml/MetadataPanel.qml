import QtQuick
import QtQuick.Layouts

// Camera metadata of the open photo, read by cybermeta.
ColumnLayout {
    id: panel
    readonly property var m: imageEditor.metadata || ({})
    readonly property bool empty: Object.keys(m).length === 0
    readonly property var gps: m.gps || null

    function exposureLine() {
        const parts = []
        if (m.aperture) parts.push("ƒ/" + Number(m.aperture).toFixed(1).replace(/\.0$/, ""))
        if (m.shutter) parts.push(m.shutter)
        if (m.iso) parts.push("ISO " + m.iso)
        if (m.focal_length) parts.push(Math.round(m.focal_length) + " mm")
        if (m.exposure_bias) parts.push((m.exposure_bias > 0 ? "+" : "") + Number(m.exposure_bias).toFixed(1) + " EV")
        return parts.join("  ·  ")
    }

    Layout.fillWidth: true
    spacing: 4

    component Row2: RowLayout {
        property string label
        property string value
        property color tint: Theme.text
        visible: value !== ""
        Layout.fillWidth: true
        Text { text: parent.label; color: Theme.muted; font.family: Theme.mono; font.pixelSize: 9; Layout.preferredWidth: 70 }
        Text { text: parent.value; color: parent.tint; font.family: Theme.mono; font.pixelSize: 10; Layout.fillWidth: true; wrapMode: Text.Wrap }
    }

    Text {
        visible: panel.empty
        text: imageEditor.metadataStatus !== "" ? imageEditor.metadataStatus : "No camera metadata in this file."
        color: Theme.muted; font.family: Theme.sans; font.pixelSize: 10; wrapMode: Text.Wrap; Layout.fillWidth: true
    }
    Row2 { label: "CAMERA"; value: panel.m.camera || "" }
    Row2 { label: "LENS"; value: panel.m.lens || "" }
    Row2 { label: "EXPOSURE"; value: panel.exposureLine(); tint: Theme.cyan }
    Row2 { label: "CAPTURED"; value: (panel.m.captured || "").replace("T", "  ") }
    Row2 { label: "ARTIST"; value: [panel.m.artist, panel.m.copyright].filter(x => x).join("  ·  ") }
    Row2 { label: "SOFTWARE"; value: panel.m.software || "" }
    Rectangle {
        visible: panel.gps !== null
        Layout.fillWidth: true; Layout.topMargin: 4
        implicitHeight: gpsBody.implicitHeight + 12
        color: Theme.alpha(Theme.pink, 0.08); border.color: Theme.pink; radius: 2
        ColumnLayout {
            id: gpsBody
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 6
            spacing: 4
            RowLayout {
                Layout.fillWidth: true
                Text { text: "⌖  LOCATION"; color: Theme.pink; font.family: Theme.mono; font.pixelSize: 9; font.bold: true; Layout.fillWidth: true }
                Chip { text: "OPEN MAP ↗"; accent: Theme.pink; onClicked: Qt.openUrlExternally(panel.gps.map_url) }
            }
            Text {
                text: panel.gps ? Number(panel.gps.latitude).toFixed(5) + ",  " + Number(panel.gps.longitude).toFixed(5)
                                  + (panel.gps.altitude !== null && panel.gps.altitude !== undefined ? "  ·  " + Math.round(panel.gps.altitude) + " m" : "") : ""
                color: Theme.text; font.family: Theme.mono; font.pixelSize: 10
            }
            Text {
                text: "This photo records where it was taken. Exports remove it unless you choose KEEP ALL."
                color: Theme.muted; font.family: Theme.sans; font.pixelSize: 9; wrapMode: Text.Wrap; Layout.fillWidth: true
            }
        }
    }
    Text {
        text: imageEditor.cybermetaVersion !== "" ? "via cybermeta " + imageEditor.cybermetaVersion : ""
        visible: text !== ""
        color: Theme.muted; font.family: Theme.mono; font.pixelSize: 8; Layout.topMargin: 2
    }
}
