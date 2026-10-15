import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Point-curve editor for curve.master / red / green / blue. Click to add a
// point, drag to move it, double-click a point to remove it. The line is
// sampled from the engine, so it matches the render exactly.
ColumnLayout {
    id: editor
    property string channel: "master"
    readonly property string key: "curve." + channel
    readonly property color accent: channel === "red" ? Theme.pink : channel === "green" ? Theme.lime
                                  : channel === "blue" ? Theme.cyan : Theme.text
    // Working copy of the points while dragging ([[x, y], ...]).
    property var points: []
    property int dragIndex: -1

    function loadPoints() {
        const stored = imageEditor.recipe[key]
        points = stored && stored.length >= 2 ? stored.map(p => [p[0], p[1]]) : [[0, 0], [1, 1]]
        canvas.requestPaint()
    }
    function push(commit, label) {
        const sorted = points.slice().sort((a, b) => a[0] - b[0])
        imageEditor.setParameter(key, sorted)
        if (commit) imageEditor.commitEdit(label)
    }

    Layout.fillWidth: true
    spacing: 6
    enabled: imageEditor.ready
    onChannelChanged: loadPoints()
    Component.onCompleted: loadPoints()
    Connections { target: imageEditor; function onRecipeChanged() { if (editor.dragIndex < 0) editor.loadPoints() } }

    RowLayout {
        Layout.fillWidth: true
        spacing: 4
        Repeater {
            model: [["master", "RGB"], ["red", "R"], ["green", "G"], ["blue", "B"]]
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true; Layout.preferredHeight: 22; radius: 2
                readonly property bool current: editor.channel === modelData[0]
                color: current ? Theme.panel2 : "transparent"
                border.color: current ? editor.accent : Theme.line
                Text { anchors.centerIn: parent; text: parent.modelData[1]; color: parent.current ? Theme.text : Theme.muted; font.family: Theme.mono; font.pixelSize: 10 }
                MouseArea { anchors.fill: parent; onClicked: editor.channel = parent.modelData[0] }
            }
        }
    }
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: width * 0.72
        color: Theme.bg; border.color: Theme.line; radius: 2
        Image { anchors.fill: parent; anchors.margins: 1; source: imageEditor.histogramUrl; opacity: 0.35; fillMode: Image.Stretch; cache: false }
        Canvas {
            id: canvas
            anchors.fill: parent; anchors.margins: 1
            renderStrategy: Canvas.Immediate
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.08); ctx.lineWidth = 1
                for (let i = 1; i < 4; ++i) {
                    ctx.beginPath(); ctx.moveTo(width * i / 4, 0); ctx.lineTo(width * i / 4, height); ctx.stroke()
                    ctx.beginPath(); ctx.moveTo(0, height * i / 4); ctx.lineTo(width, height * i / 4); ctx.stroke()
                }
                ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.15)
                ctx.beginPath(); ctx.moveTo(0, height); ctx.lineTo(width, 0); ctx.stroke()
                const samples = imageEditor.curveSamples(editor.key, 96)
                ctx.strokeStyle = editor.accent; ctx.lineWidth = 2
                ctx.beginPath()
                for (let i = 0; i < samples.length; ++i) {
                    const x = width * i / (samples.length - 1), y = height * (1 - samples[i])
                    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                }
                ctx.stroke()
                ctx.fillStyle = Theme.bg
                for (const p of editor.points) {
                    ctx.beginPath(); ctx.rect(p[0] * width - 4, (1 - p[1]) * height - 4, 8, 8)
                    ctx.fill(); ctx.stroke()
                }
            }
            Connections { target: imageEditor; function onRecipeChanged() { canvas.requestPaint() } function onPreviewChanged() { canvas.requestPaint() } }
        }
        MouseArea {
            anchors.fill: canvas
            function nearest(mx, my) {
                for (let i = 0; i < editor.points.length; ++i) {
                    const p = editor.points[i]
                    if (Math.abs(p[0] * width - mx) < 9 && Math.abs((1 - p[1]) * height - my) < 9) return i
                }
                return -1
            }
            function clampPoint(mx, my) { return [Math.max(0, Math.min(1, mx / width)), Math.max(0, Math.min(1, 1 - my / height))] }
            onPressed: (mouse) => {
                let i = nearest(mouse.x, mouse.y)
                if (i < 0) { editor.points.push(clampPoint(mouse.x, mouse.y)); i = editor.points.length - 1 }
                editor.dragIndex = i
                editor.push(false)
            }
            onPositionChanged: (mouse) => {
                if (editor.dragIndex < 0) return
                editor.points[editor.dragIndex] = clampPoint(mouse.x, mouse.y)
                editor.push(false)
                canvas.requestPaint()
            }
            onReleased: { if (editor.dragIndex >= 0) { editor.dragIndex = -1; editor.push(true, "Curve " + editor.channel) } }
            onDoubleClicked: (mouse) => {
                const i = nearest(mouse.x, mouse.y)
                if (i >= 0 && editor.points.length > 2) { editor.points.splice(i, 1); editor.push(true, "Remove curve point") }
            }
        }
    }
    Text { text: "Click to add · drag to move · double-click a point to remove"; color: Theme.muted; font.family: Theme.sans; font.pixelSize: 9 }
}
