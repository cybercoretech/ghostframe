import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// GHOSTFRAME // IMAGE LAB: preview with drag-to-crop, adjustments, undo/redo
// and the export + Cybermeta form. All state lives in `imageEditor` (C++).
Rectangle {
    id: editor
    signal closeRequested()

    color: "#f0080a0d"

    Rectangle {
        anchors.fill: parent; anchors.margins: 24
        color: Theme.panel; border.color: Theme.cyan; border.width: 1; radius: 3
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 18; spacing: 12
            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 3
                    Text { text: "GHOSTFRAME // IMAGE LAB"; color: Theme.cyan; font.family: Theme.mono; font.pixelSize: 16; font.bold: true; font.letterSpacing: 1.2 }
                    Text { text: imageEditor.sourceTitle + "  ·  Full-resolution nondestructive export"; color: Theme.muted; font.family: Theme.sans; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
                }
                EditorButton { text: "↶  UNDO"; primary: false; accent: Theme.cyan; enabled: imageEditor.ready && imageEditor.canUndo; onClicked: imageEditor.undo(); ToolTip.visible: hovered; ToolTip.text: "Ctrl+Z" }
                EditorButton { text: "REDO  ↷"; primary: false; accent: Theme.cyan; enabled: imageEditor.ready && imageEditor.canRedo; onClicked: imageEditor.redo(); ToolTip.visible: hovered; ToolTip.text: "Ctrl+Shift+Z" }
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
                Rectangle {
                    id: stage
                    Layout.fillWidth: true; Layout.fillHeight: true
                    color: "#07090c"; border.color: Theme.line; radius: 2; clip: true
                    readonly property real aspect: imageEditor.displayWidth > 0 && imageEditor.displayHeight > 0
                                                   ? imageEditor.displayWidth / imageEditor.displayHeight : 0
                    Image {
                        id: preview
                        anchors.centerIn: parent
                        width: stage.aspect > 0 ? Math.min(parent.width - 2, (parent.height - 2) * stage.aspect) : parent.width
                        height: stage.aspect > 0 ? Math.min(parent.height - 2, (parent.width - 2) / stage.aspect) : parent.height
                        source: imageEditor.previewUrl
                        fillMode: Image.PreserveAspectFit
                        // The provider returns an in-memory frame, so a synchronous
                        // load is instant and avoids flicker between frames.
                        asynchronous: false; cache: false
                    }
                    // Crop selection, drawn on top of the rendered result.
                    Rectangle {
                        id: cropBox
                        visible: dragArea.dragging
                        color: Theme.alpha(Theme.cyan, 0.10); border.color: Theme.cyan; border.width: 2
                    }
                    MouseArea {
                        id: dragArea
                        anchors.fill: preview
                        enabled: imageEditor.ready && preview.status === Image.Ready
                        cursorShape: Qt.CrossCursor
                        property point start
                        property bool dragging: false
                        onPressed: (mouse) => { start = Qt.point(mouse.x, mouse.y); dragging = true; update(mouse) }
                        onPositionChanged: (mouse) => update(mouse)
                        function update(mouse) {
                            const x = Math.max(0, Math.min(mouse.x, width)), y = Math.max(0, Math.min(mouse.y, height))
                            cropBox.x = preview.x + Math.min(start.x, x); cropBox.y = preview.y + Math.min(start.y, y)
                            cropBox.width = Math.abs(x - start.x); cropBox.height = Math.abs(y - start.y)
                        }
                        onReleased: (mouse) => {
                            dragging = false
                            const x = Math.max(0, Math.min(mouse.x, width)), y = Math.max(0, Math.min(mouse.y, height))
                            const w = Math.abs(x - start.x) / width, h = Math.abs(y - start.y) / height
                            if (w <= 0.015 || h <= 0.015) return
                            // The preview already shows the current crop, so map the
                            // selection back into the uncropped frame.
                            const base = imageEditor.hasCrop
                                ? { x: imageEditor.cropX, y: imageEditor.cropY, w: imageEditor.cropWidth, h: imageEditor.cropHeight }
                                : { x: 0, y: 0, w: 1, h: 1 }
                            imageEditor.setCrop(base.x + Math.min(start.x, x) / width * base.w,
                                                base.y + Math.min(start.y, y) / height * base.h,
                                                w * base.w, h * base.h)
                        }
                    }
                    Text {
                        anchors.centerIn: parent; visible: preview.status !== Image.Ready
                        text: imageEditor.status; color: Theme.muted; font.family: Theme.mono; font.pixelSize: 11
                        wrapMode: Text.Wrap; width: parent.width - 40; horizontalAlignment: Text.AlignHCenter
                    }
                }
                ScrollView {
                    Layout.preferredWidth: 330; Layout.fillHeight: true; clip: true
                    ColumnLayout {
                        width: 310; spacing: 10
                        Text { text: "TRANSFORM"; color: Theme.lime; font.family: Theme.mono; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1 }
                        RowLayout {
                            Layout.fillWidth: true
                            EditorButton { text: "↶  ROTATE LEFT"; Layout.fillWidth: true; enabled: imageEditor.ready; onClicked: imageEditor.rotateBy(-90) }
                            EditorButton { text: "ROTATE RIGHT  ↷"; Layout.fillWidth: true; enabled: imageEditor.ready; onClicked: imageEditor.rotateBy(90) }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            EditorButton { text: "CLEAR CROP"; primary: false; Layout.fillWidth: true; enabled: imageEditor.hasCrop; onClicked: imageEditor.clearCrop() }
                            EditorButton { text: "RESET ALL"; primary: false; accent: Theme.pink; Layout.fillWidth: true; enabled: imageEditor.ready; onClicked: imageEditor.resetEdits() }
                        }
                        Text { text: imageEditor.hasCrop ? "Drag again to crop further · Clear crop to start over" : "Drag over the preview to crop"; color: Theme.muted; font.family: Theme.sans; font.pixelSize: 10 }

                        Text { text: "TONE"; color: Theme.lime; font.family: Theme.mono; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1; Layout.topMargin: 4 }
                        AdjustSlider {
                            id: lightSlider
                            label: "LIGHT"; from: -50; to: 50; value: imageEditor.brightness
                            onMoved: (v) => imageEditor.setAdjustments(v, contrastSlider.liveValue, saturationSlider.liveValue)
                            onCommitted: imageEditor.commitEdit()
                        }
                        AdjustSlider {
                            id: contrastSlider
                            label: "CONTRAST"; from: -50; to: 50; value: imageEditor.contrast
                            onMoved: (v) => imageEditor.setAdjustments(lightSlider.liveValue, v, saturationSlider.liveValue)
                            onCommitted: imageEditor.commitEdit()
                        }
                        AdjustSlider {
                            id: saturationSlider
                            label: "SATURATION"; suffix: "%"; from: 0; to: 200; value: imageEditor.saturation; accent: Theme.pink
                            onMoved: (v) => imageEditor.setAdjustments(lightSlider.liveValue, contrastSlider.liveValue, v)
                            onCommitted: imageEditor.commitEdit()
                        }

                        Text { text: "CYBERMETA"; color: Theme.pink; font.family: Theme.mono; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1; Layout.topMargin: 5 }
                        MetaField { id: metaTitle; Layout.fillWidth: true; text: imageEditor.sourceTitle; placeholderText: "Title" }
                        MetaField { id: metaCaption; Layout.fillWidth: true; placeholderText: "Caption / description" }
                        MetaField { id: metaTags; Layout.fillWidth: true; placeholderText: "Tags, separated by commas" }
                        Text { text: "Writes an XMP sidecar with the source hash, folder, the full edit recipe, title, caption and tags."; color: Theme.muted; font.family: Theme.sans; font.pixelSize: 9; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        EditorButton {
                            Layout.fillWidth: true; Layout.preferredHeight: 42
                            text: imageEditor.busy ? "WORKING…" : "SAVE EXPORT + CYBERMETA"
                            accent: Theme.lime
                            enabled: imageEditor.ready && !imageEditor.busy
                            onClicked: imageEditor.save(metaTitle.text, metaCaption.text, metaTags.text)
                        }
                        Text { text: imageEditor.status; color: Theme.cyan; font.family: Theme.sans; font.pixelSize: 10; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    }
                }
            }
        }
    }
}
