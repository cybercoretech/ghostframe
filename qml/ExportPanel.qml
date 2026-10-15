import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Export presets: size + fit, format and quality. Exported files are new
// images in ~/Pictures/Ghostframe/Exports; the original is never touched.
ColumnLayout {
    id: panel
    property string preset: "original"
    property string fit: "fill"
    property string format: ""
    property int quality: 92
    readonly property var presets: [
        { id: "original", label: "ORIGINAL", w: 0, h: 0 },
        { id: "shorts", label: "SHORTS 9:16", w: 1080, h: 1920 },
        { id: "thumbnail", label: "THUMB 16:9", w: 1280, h: 720 },
        { id: "square", label: "SQUARE", w: 1080, h: 1080 },
        { id: "web", label: "WEB 2560", w: 0, h: 0 }]
    readonly property var current: presets.find(p => p.id === preset)

    // {preset, width, height, fit, format, quality} for imageEditor.save().
    function options() {
        let w = current.w, h = current.h
        if (preset === "web" && imageEditor.displayWidth > 0) {
            const s = Math.min(1, 2560 / Math.max(imageEditor.displayWidth, imageEditor.displayHeight))
            w = Math.round(imageEditor.displayWidth * s); h = Math.round(imageEditor.displayHeight * s)
        }
        return { preset: preset === "original" ? "" : preset, width: w, height: h, fit: fit,
                 format: format !== "" ? format : (preset === "web" ? "webp" : ""), quality: quality }
    }

    Layout.fillWidth: true
    spacing: 6
    Text { text: "SIZE"; color: Theme.muted; font.family: Theme.mono; font.pixelSize: 9 }
    Flow {
        Layout.fillWidth: true; spacing: 4
        Repeater {
            model: panel.presets
            delegate: Chip { required property var modelData; text: modelData.label; accent: Theme.lime
                             selected: panel.preset === modelData.id; onClicked: panel.preset = modelData.id }
        }
    }
    Flow {
        Layout.fillWidth: true; spacing: 4
        visible: panel.current.w > 0
        Repeater {
            model: [["fill", "FILL (CROP)"], ["blur", "FIT + BLUR"], ["pad", "FIT + BARS"]]
            delegate: Chip { required property var modelData; text: modelData[1]; accent: Theme.cyan
                             selected: panel.fit === modelData[0]; onClicked: panel.fit = modelData[0] }
        }
    }
    Text { text: "FORMAT"; color: Theme.muted; font.family: Theme.mono; font.pixelSize: 9 }
    Flow {
        Layout.fillWidth: true; spacing: 4
        Repeater {
            model: [["", "AUTO"], ["jpg", "JPG"], ["png", "PNG"], ["webp", "WEBP"], ["avif", "AVIF"]]
            delegate: Chip { required property var modelData; text: modelData[1]; accent: Theme.pink
                             selected: panel.format === modelData[0]; onClicked: panel.format = modelData[0] }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        visible: panel.format !== "png"
        Text { text: "QUALITY"; color: Theme.muted; font.family: Theme.mono; font.pixelSize: 9; Layout.preferredWidth: 62 }
        MiniSlider { id: q; accent: Theme.pink; Layout.fillWidth: true; from: 40; to: 100; stepSize: 1; value: panel.quality; onMoved: panel.quality = value }
        Text { text: panel.quality; color: Theme.cyan; font.family: Theme.mono; font.pixelSize: 9; Layout.preferredWidth: 24 }
    }
    Text {
        text: {
            const o = panel.options()
            const size = o.width > 0 ? o.width + " × " + o.height : (imageEditor.displayWidth + " × " + imageEditor.displayHeight)
            return "→ " + size + " px · " + (o.format !== "" ? o.format.toUpperCase() : "same format as the original")
        }
        color: Theme.muted; font.family: Theme.mono; font.pixelSize: 9
    }
}
