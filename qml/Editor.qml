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
                    objectName: "editorPanel"
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

                        // ── metadata ──
                        Section {
                            title: "METADATA"; accent: Theme.cyan
                            MetadataPanel { Layout.fillWidth: true }
                        }

                        // ── looks ──
                        Section {
                            title: "LOOKS"; accent: Theme.cyan
                            LookPanel { Layout.fillWidth: true }
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

                        // ── cyber fx ──
                        Section {
                            title: "CYBERGRID GRADIENT"; accent: Theme.pink
                            CyberGridPanel { Layout.fillWidth: true }
                        }
                        Section {
                            title: "FX"; accent: Theme.pink
                            resetKeys: ["bloomAmount", "bloomThreshold", "bloomRadius", "aberrationAmount", "glitchAmount", "glitchSeed",
                                        "pixelSortAmount", "pixelSortVertical", "halftoneAmount", "halftoneSize", "scanlineAmount",
                                        "scanlineSpacing", "grainAmount", "grainSize", "vignetteAmount", "vignetteMidpoint"]
                            ParamSlider { key: "bloomAmount"; label: "NEON BLOOM"; from: 0; to: 100; accent: Theme.pink }
                            ParamSlider { key: "bloomThreshold"; label: "BLOOM THRESHOLD"; from: 0; to: 100; defaultValue: 70; accent: Theme.pink; visible: (imageEditor.recipe.bloomAmount || 0) > 0 }
                            ParamSlider { key: "bloomRadius"; label: "BLOOM RADIUS"; from: 1; to: 120; defaultValue: 20; suffix: " px"; accent: Theme.pink; visible: (imageEditor.recipe.bloomAmount || 0) > 0 }
                            ParamSlider { key: "aberrationAmount"; label: "CHROMATIC ABERRATION"; from: 0; to: 100 }
                            ParamSlider { key: "glitchAmount"; label: "GLITCH"; from: 0; to: 100 }
                            RowLayout {
                                Layout.fillWidth: true
                                visible: (imageEditor.recipe.glitchAmount || 0) > 0
                                Text { text: "SEED " + Math.round(imageEditor.recipe.glitchSeed || 0); color: Theme.muted; font.family: Theme.mono; font.pixelSize: 9; Layout.fillWidth: true }
                                Chip { text: "↻ RESEED"; onClicked: imageEditor.applyParameter("glitchSeed", Math.floor(Math.random() * 1000), "Glitch seed") }
                            }
                            ParamSlider { key: "pixelSortAmount"; label: "PIXEL SORT"; from: 0; to: 100; accent: Theme.lime }
                            RowLayout {
                                visible: (imageEditor.recipe.pixelSortAmount || 0) > 0
                                Chip { text: "↔ ROWS"; accent: Theme.lime; selected: !imageEditor.recipe.pixelSortVertical; onClicked: imageEditor.applyParameter("pixelSortVertical", false, "Pixel sort rows") }
                                Chip { text: "↕ COLUMNS"; accent: Theme.lime; selected: imageEditor.recipe.pixelSortVertical === true; onClicked: imageEditor.applyParameter("pixelSortVertical", true, "Pixel sort columns") }
                            }
                            ParamSlider { key: "halftoneAmount"; label: "HALFTONE"; from: 0; to: 100; accent: Theme.lime }
                            ParamSlider { key: "halftoneSize"; label: "DOT SIZE"; from: 3; to: 60; defaultValue: 8; suffix: " px"; accent: Theme.lime; visible: (imageEditor.recipe.halftoneAmount || 0) > 0 }
                            ParamSlider { key: "scanlineAmount"; label: "SCANLINES"; from: 0; to: 100 }
                            ParamSlider { key: "scanlineSpacing"; label: "LINE SPACING"; from: 2; to: 24; defaultValue: 4; suffix: " px"; visible: (imageEditor.recipe.scanlineAmount || 0) > 0 }
                            ParamSlider { key: "grainAmount"; label: "FILM GRAIN"; from: 0; to: 100 }
                            ParamSlider { key: "grainSize"; label: "GRAIN SIZE"; from: 0.5; to: 4; stepSize: 0.1; decimals: 1; defaultValue: 1.5; suffix: " px"; visible: (imageEditor.recipe.grainAmount || 0) > 0 }
                            ParamSlider { key: "vignetteAmount"; label: "VIGNETTE" }
                            ParamSlider { key: "vignetteMidpoint"; label: "VIGNETTE MIDPOINT"; from: 0; to: 100; defaultValue: 50; visible: (imageEditor.recipe.vignetteAmount || 0) !== 0 }
                        }
                        Section {
                            title: "LUT"; expanded: false
                            resetKeys: ["lutName", "lutAmount"]
                            LutPanel { Layout.fillWidth: true }
                        }
                        Section {
                            title: "TEXT & WATERMARK"; accent: Theme.cyan
                            TextLayersPanel { Layout.fillWidth: true }
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
                            title: "CYBERMETA + EXPORT"; accent: Theme.lime
                            MetaField { id: metaTitle; Layout.fillWidth: true; text: imageEditor.sourceTitle; placeholderText: "Title" }
                            MetaField { id: metaCaption; Layout.fillWidth: true; placeholderText: "Caption / description" }
                            MetaField { id: metaTags; Layout.fillWidth: true; placeholderText: "Tags, separated by commas" }
                            Text { text: "Title, caption and tags go inside the exported file (XMP, plus the caption as EXIF description) and into a .xmp sidecar with the source and the full edit recipe, all written by cybermeta. Edits are also remembered per photo."; color: Theme.muted; font.family: Theme.sans; font.pixelSize: 9; wrapMode: Text.Wrap; Layout.fillWidth: true }
                            ExportPanel { id: exportPanel; Layout.fillWidth: true }
                            EditorButton {
                                Layout.fillWidth: true; Layout.preferredHeight: 42
                                text: imageEditor.busy ? "WORKING…" : "SAVE EXPORT + CYBERMETA"
                                accent: Theme.lime
                                enabled: imageEditor.ready && !imageEditor.busy
                                onClicked: imageEditor.save(metaTitle.text, metaCaption.text, metaTags.text, exportPanel.options())
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
