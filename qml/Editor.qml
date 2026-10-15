import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// GHOSTFRAME // IMAGE LAB. All edit state lives in `imageEditor` (C++); this
// file is layout: toolbar, the picture (EditorStage) and the adjustment
// panel. Keyboard shortcuts are wired in Main.qml.
Rectangle {
    id: editor
    signal closeRequested()
    signal navigateRequested(int delta)
    readonly property alias stage: stage

    color: "#f0080a0d"

    readonly property var hslKeys: {
        const keys = []
        for (const band of ["red", "orange", "yellow", "green", "aqua", "blue", "purple", "magenta"])
            for (const part of ["hue", "saturation", "luminance"]) keys.push("hsl." + band + "." + part)
        return keys
    }

    Rectangle {
        anchors.fill: parent; anchors.margins: 24
        color: Theme.panel; border.color: Theme.cyan; border.width: 1; radius: 3
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 18; spacing: 12

            // ── toolbar ──
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 3
                    Text { text: "GHOSTFRAME // IMAGE LAB"; color: Theme.cyan; font.family: Theme.mono; font.pixelSize: 16; font.bold: true; font.letterSpacing: 1.2 }
                    Text { text: imageEditor.sourceTitle + "  ·  Full-resolution nondestructive export"; color: Theme.muted; font.family: Theme.sans; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
                }
                EditorButton { text: "‹"; primary: false; implicitWidth: 30; enabled: imageEditor.ready; onClicked: editor.navigateRequested(-1); ToolTip.visible: hovered; ToolTip.text: "Previous photo  [" }
                EditorButton { text: "›"; primary: false; implicitWidth: 30; enabled: imageEditor.ready; onClicked: editor.navigateRequested(1); ToolTip.visible: hovered; ToolTip.text: "Next photo  ]" }
                EditorButton { text: "⌗  CROP"; primary: imageEditor.cropMode; accent: Theme.lime; enabled: imageEditor.ready; onClicked: imageEditor.cropMode = !imageEditor.cropMode; ToolTip.visible: hovered; ToolTip.text: "R" }
                EditorButton { text: "◧  BEFORE"; primary: imageEditor.showBefore; enabled: imageEditor.ready; onClicked: imageEditor.showBefore = !imageEditor.showBefore; ToolTip.visible: hovered; ToolTip.text: "\\" }
                EditorButton { text: "◮  CLIPPING"; primary: imageEditor.showClipping; accent: Theme.pink; enabled: imageEditor.ready; onClicked: imageEditor.showClipping = !imageEditor.showClipping; ToolTip.visible: hovered; ToolTip.text: "J · pink = blown highlights, cyan = crushed shadows" }
                EditorButton { text: "⊕  1:1"; primary: imageEditor.zoomed; enabled: imageEditor.ready && !imageEditor.cropMode; onClicked: stage.toggleZoom(); ToolTip.visible: hovered; ToolTip.text: "Z" }
                EditorButton { text: "↶  UNDO"; primary: false; enabled: imageEditor.ready && imageEditor.canUndo; onClicked: imageEditor.undo(); ToolTip.visible: hovered; ToolTip.text: "Ctrl+Z" }
                EditorButton { text: "REDO  ↷"; primary: false; enabled: imageEditor.ready && imageEditor.canRedo; onClicked: imageEditor.redo(); ToolTip.visible: hovered; ToolTip.text: "Ctrl+Shift+Z" }
                Button {
                    id: closeButton
                    text: "×"; Accessible.name: "Close editor"
                    onClicked: editor.closeRequested()
                    contentItem: Text { text: "×"; color: Theme.darkText; font.pixelSize: 23; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: closeButton.hovered ? Qt.lighter(Theme.pink, 1.14) : Theme.pink; border.color: Theme.darkText; radius: 2 }
                }
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.line }

            RowLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 16

                EditorStage { id: stage; Layout.fillWidth: true; Layout.fillHeight: true }

                ScrollView {
                    id: panel
                    Layout.preferredWidth: 340; Layout.fillHeight: true; clip: true
                    contentWidth: availableWidth
                    ColumnLayout {
                        width: panel.availableWidth - 12; spacing: 8

                        // ── histogram ──
                        Rectangle {
                            Layout.fillWidth: true; Layout.preferredHeight: 92
                            color: Theme.bg; border.color: Theme.line; radius: 2
                            Image { anchors.fill: parent; anchors.margins: 1; source: imageEditor.histogramUrl; fillMode: Image.Stretch; cache: false; asynchronous: false }
                        }
                        Text {
                            text: imageEditor.ready ? imageEditor.displayWidth + " × " + imageEditor.displayHeight + " px  ·  " + imageEditor.lastRenderMs + " ms" : "—"
                            color: Theme.muted; font.family: Theme.mono; font.pixelSize: 9
                        }

                        // ── transform ──
                        Section {
                            title: "TRANSFORM"
                            resetKeys: ["straighten", "perspectiveVertical", "perspectiveHorizontal", "flipHorizontal", "flipVertical"]
                            RowLayout {
                                Layout.fillWidth: true
                                EditorButton { text: "↶ ROTATE"; Layout.fillWidth: true; enabled: imageEditor.ready; onClicked: imageEditor.rotateBy(-90) }
                                EditorButton { text: "ROTATE ↷"; Layout.fillWidth: true; enabled: imageEditor.ready; onClicked: imageEditor.rotateBy(90) }
                                EditorButton { text: "⇋ FLIP"; primary: imageEditor.recipe.flipHorizontal; Layout.fillWidth: true; enabled: imageEditor.ready; onClicked: imageEditor.flip(true) }
                                EditorButton { text: "⇵ FLIP"; primary: imageEditor.recipe.flipVertical; Layout.fillWidth: true; enabled: imageEditor.ready; onClicked: imageEditor.flip(false) }
                            }
                            ParamSlider { key: "straighten"; label: "STRAIGHTEN"; from: -45; to: 45; stepSize: 0.1; decimals: 1; suffix: "°" }
                            ParamSlider { key: "perspectiveVertical"; label: "VERTICAL PERSPECTIVE" }
                            ParamSlider { key: "perspectiveHorizontal"; label: "HORIZONTAL PERSPECTIVE" }
                            Text { text: "CROP"; color: Theme.muted; font.family: Theme.mono; font.pixelSize: 9; Layout.topMargin: 2 }
                            Flow {
                                Layout.fillWidth: true; spacing: 4
                                Repeater {
                                    model: [["FREE", 0], ["ORIGINAL", -1], ["1:1", 1], ["4:5", 0.8], ["3:2", 1.5], ["16:9", 16 / 9], ["9:16", 9 / 16]]
                                    delegate: EditorButton {
                                        required property var modelData
                                        readonly property real ratio: modelData[1] < 0
                                            ? (imageEditor.frameHeight > 0 ? imageEditor.frameWidth / imageEditor.frameHeight : 0) : modelData[1]
                                        text: modelData[0]; implicitHeight: 24
                                        primary: Math.abs(stage.cropAspect - ratio) < 1e-6
                                        accent: Theme.lime
                                        enabled: imageEditor.ready
                                        onClicked: { imageEditor.cropMode = true; stage.applyAspect(ratio) }
                                    }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                EditorButton { text: imageEditor.cropMode ? "✓  DONE" : "⌗  EDIT CROP"; primary: imageEditor.cropMode; accent: Theme.lime; Layout.fillWidth: true; enabled: imageEditor.ready; onClicked: imageEditor.cropMode = !imageEditor.cropMode }
                                EditorButton { text: "CLEAR CROP"; primary: false; Layout.fillWidth: true; enabled: imageEditor.hasCrop; onClicked: imageEditor.clearCrop() }
                            }
                        }

                        // ── light ──
                        Section {
                            title: "LIGHT"
                            resetKeys: ["exposure", "contrast", "highlights", "shadows", "whites", "blacks", "brightness"]
                            ParamSlider { key: "exposure"; label: "EXPOSURE"; from: -5; to: 5; stepSize: 0.05; decimals: 2; suffix: " EV" }
                            ParamSlider { key: "contrast"; label: "CONTRAST"; from: -60; to: 60 }
                            ParamSlider { key: "highlights"; label: "HIGHLIGHTS"; accent: Theme.pink }
                            ParamSlider { key: "shadows"; label: "SHADOWS"; accent: Theme.pink }
                            ParamSlider { key: "whites"; label: "WHITES" }
                            ParamSlider { key: "blacks"; label: "BLACKS" }
                            ParamSlider { key: "brightness"; label: "LIGHT"; from: -60; to: 60 }
                        }

                        // ── levels ──
                        Section {
                            title: "LEVELS"; expanded: false
                            resetKeys: ["levelsBlack", "levelsWhite", "levelsGamma"]
                            ParamSlider { key: "levelsBlack"; label: "INPUT BLACK"; from: 0; to: 0.5; stepSize: 0.002; displayScale: 255 }
                            ParamSlider { key: "levelsGamma"; label: "GAMMA"; from: 0.2; to: 3; stepSize: 0.01; decimals: 2; defaultValue: 1 }
                            ParamSlider { key: "levelsWhite"; label: "INPUT WHITE"; from: 0.5; to: 1; stepSize: 0.002; displayScale: 255; defaultValue: 1 }
                        }

                        // ── curve ──
                        Section {
                            title: "CURVE"
                            resetKeys: ["curve.master", "curve.red", "curve.green", "curve.blue"]
                            CurveEditor { Layout.fillWidth: true }
                        }

                        // ── colour ──
                        Section {
                            title: "COLOUR"; accent: Theme.pink
                            resetKeys: ["temperature", "tint", "vibrance", "saturation"]
                            ParamSlider { key: "temperature"; label: "TEMPERATURE"; accent: "#ffb347" }
                            ParamSlider { key: "tint"; label: "TINT"; accent: Theme.pink }
                            ParamSlider { key: "vibrance"; label: "VIBRANCE"; accent: Theme.pink }
                            ParamSlider { key: "saturation"; label: "SATURATION"; from: 0; to: 200; defaultValue: 100; suffix: "%"; accent: Theme.pink }
                        }

                        // ── hsl ──
                        Section {
                            title: "HSL"; accent: Theme.pink
                            resetKeys: editor.hslKeys
                            HslPanel { Layout.fillWidth: true }
                        }

                        // ── detail ──
                        Section {
                            title: "DETAIL"; expanded: false
                            resetKeys: ["sharpenAmount", "sharpenRadius", "sharpenThreshold", "noiseLuminance", "noiseColor"]
                            ParamSlider { key: "sharpenAmount"; label: "SHARPEN"; from: 0; to: 300; suffix: "%" }
                            ParamSlider { key: "sharpenRadius"; label: "RADIUS"; from: 0.5; to: 3; stepSize: 0.1; decimals: 1; defaultValue: 1; suffix: " px" }
                            ParamSlider { key: "sharpenThreshold"; label: "THRESHOLD"; from: 0; to: 100 }
                            ParamSlider { key: "noiseLuminance"; label: "LUMINANCE NOISE"; from: 0; to: 100; accent: Theme.lime }
                            ParamSlider { key: "noiseColor"; label: "COLOUR NOISE"; from: 0; to: 100; accent: Theme.lime }
                            Text { text: "Use 1:1 zoom (Z) to judge detail."; color: Theme.muted; font.family: Theme.sans; font.pixelSize: 9 }
                        }

                        // ── history ──
                        Section {
                            title: "HISTORY"; expanded: false
                            Repeater {
                                model: imageEditor.historyLabels
                                delegate: Rectangle {
                                    required property string modelData
                                    required property int index
                                    readonly property bool current: index === imageEditor.historyIndex
                                    Layout.fillWidth: true; Layout.preferredHeight: 24; radius: 2
                                    color: current ? Theme.panel2 : historyHover.containsMouse ? Theme.panelHover : "transparent"
                                    border.color: current ? Theme.cyan : "transparent"
                                    opacity: index > imageEditor.historyIndex ? 0.5 : 1
                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 8
                                        text: (parent.index + 1) + "  ·  " + parent.modelData
                                        color: parent.current ? Theme.text : Theme.muted; font.family: Theme.mono; font.pixelSize: 10
                                    }
                                    MouseArea { id: historyHover; anchors.fill: parent; hoverEnabled: true; onClicked: imageEditor.jumpToHistory(parent.index) }
                                }
                            }
                        }

                        // ── cybermeta + export ──
                        Section {
                            title: "CYBERMETA"; accent: Theme.pink
                            MetaField { id: metaTitle; Layout.fillWidth: true; text: imageEditor.sourceTitle; placeholderText: "Title" }
                            MetaField { id: metaCaption; Layout.fillWidth: true; placeholderText: "Caption / description" }
                            MetaField { id: metaTags; Layout.fillWidth: true; placeholderText: "Tags, separated by commas" }
                            Text { text: "Writes an XMP sidecar with the source hash, folder, the full edit recipe, title, caption and tags. Edits are also remembered per photo."; color: Theme.muted; font.family: Theme.sans; font.pixelSize: 9; wrapMode: Text.Wrap; Layout.fillWidth: true }
                            EditorButton {
                                Layout.fillWidth: true; Layout.preferredHeight: 42
                                text: imageEditor.busy ? "WORKING…" : "SAVE EXPORT + CYBERMETA"
                                accent: Theme.lime
                                enabled: imageEditor.ready && !imageEditor.busy
                                onClicked: imageEditor.save(metaTitle.text, metaCaption.text, metaTags.text)
                            }
                            EditorButton { text: "RESET ALL EDITS"; primary: false; accent: Theme.pink; Layout.fillWidth: true; enabled: imageEditor.ready; onClicked: imageEditor.resetEdits() }
                        }
                        Text { text: imageEditor.status; color: Theme.cyan; font.family: Theme.sans; font.pixelSize: 10; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    }
                }
            }
        }
    }
}
