import QtQuick
import QtQuick.Controls

// The editor's picture area. Modes:
//  · normal: rendered preview; double-click (or Z) zooms to 1:1 there.
//  · before/after: the unedited frame on the left of a draggable divider.
//  · crop: the whole frame with the crop drawn on it. Drag outside the box
//    to draw a new one (locked to `cropAspect` if set), drag inside to move.
//  · 1:1 zoom: real full-resolution pixels; drag to pan.
Rectangle {
    id: stage
    // Output-pixel width/height ratio for new crops: 0 = free.
    property real cropAspect: 0
    property real splitPosition: 0.5
    readonly property real aspect: imageEditor.displayWidth > 0 && imageEditor.displayHeight > 0
                                   ? imageEditor.displayWidth / imageEditor.displayHeight : 0

    function toggleZoom(px, py) {
        if (imageEditor.zoomed) { imageEditor.setZoom(false, 0, 0, 0, 0); return }
        const cx = px === undefined ? 0.5 : (px - preview.x) / preview.width
        const cy = py === undefined ? 0.5 : (py - preview.y) / preview.height
        imageEditor.setZoom(true, cx, cy, Math.round(stage.width), Math.round(stage.height))
    }
    // Fits the current (or a centred, maximal) crop to `ratio`.
    function applyAspect(ratio) {
        cropAspect = ratio
        if (ratio <= 0 || imageEditor.frameWidth <= 0) return
        const fw = imageEditor.frameWidth, fh = imageEditor.frameHeight
        const base = imageEditor.hasCrop
            ? { x: imageEditor.cropX * fw, y: imageEditor.cropY * fh, w: imageEditor.cropWidth * fw, h: imageEditor.cropHeight * fh }
            : { x: 0, y: 0, w: fw, h: fh }
        let w = base.w, h = base.h
        if (w / h > ratio) w = h * ratio; else h = w / ratio
        const cx = base.x + base.w / 2, cy = base.y + base.h / 2
        const x = Math.max(0, Math.min(fw - w, cx - w / 2)), y = Math.max(0, Math.min(fh - h, cy - h / 2))
        imageEditor.setCrop(x / fw, y / fh, w / fw, h / fh)
    }

    color: "#07090c"; border.color: imageEditor.cropMode ? Theme.lime : Theme.line; radius: 2; clip: true

    Image {
        id: preview
        anchors.centerIn: parent
        width: stage.aspect > 0 ? Math.min(parent.width - 2, (parent.height - 2) * stage.aspect) : parent.width
        height: stage.aspect > 0 ? Math.min(parent.height - 2, (parent.width - 2) / stage.aspect) : parent.height
        source: imageEditor.previewUrl
        fillMode: Image.Stretch
        // In-memory frames from the provider: a synchronous load is instant
        // and avoids flicker between frames.
        asynchronous: false; cache: false
        visible: !imageEditor.zoomed
    }

    // ── before / after ──
    Item {
        visible: imageEditor.showBefore && !imageEditor.zoomed && !imageEditor.cropMode && imageEditor.beforeUrl.toString() !== ""
        x: preview.x; y: preview.y; width: preview.width * stage.splitPosition; height: preview.height
        clip: true
        Image { width: preview.width; height: preview.height; source: imageEditor.beforeUrl; fillMode: Image.Stretch; asynchronous: false; cache: false }
        Text { anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 8; text: "BEFORE"; color: Theme.text; font.family: Theme.mono; font.pixelSize: 10; style: Text.Outline; styleColor: Theme.bg }
    }
    Rectangle {
        id: divider
        visible: imageEditor.showBefore && !imageEditor.zoomed && !imageEditor.cropMode
        x: preview.x + preview.width * stage.splitPosition - 1; y: preview.y
        width: 2; height: preview.height; color: Theme.cyan
        Rectangle { anchors.centerIn: parent; width: 14; height: 30; radius: 2; color: Theme.bg; border.color: Theme.cyan; border.width: 2 }
        MouseArea {
            anchors.centerIn: parent; width: 24; height: parent.height
            cursorShape: Qt.SplitHCursor
            onPositionChanged: (mouse) => {
                const p = mapToItem(stage, mouse.x, mouse.y)
                stage.splitPosition = Math.max(0.02, Math.min(0.98, (p.x - preview.x) / preview.width))
            }
        }
    }
    Text {
        visible: divider.visible
        anchors.right: preview.right; anchors.top: preview.top; anchors.margins: 8
        text: "AFTER"; color: Theme.cyan; font.family: Theme.mono; font.pixelSize: 10; style: Text.Outline; styleColor: Theme.bg
    }

    // ── crop ──
    Item {
        id: cropLayer
        visible: imageEditor.cropMode
        x: preview.x; y: preview.y; width: preview.width; height: preview.height
        // The box in preview pixels: live while dragging, else the recipe's crop.
        property rect live: Qt.rect(0, 0, 0, 0)
        property bool dragging: false
        readonly property rect box: dragging ? live
            : imageEditor.hasCrop ? Qt.rect(imageEditor.cropX * width, imageEditor.cropY * height, imageEditor.cropWidth * width, imageEditor.cropHeight * height)
            : Qt.rect(0, 0, width, height)
        // Dim everything outside the box.
        Rectangle { x: 0; y: 0; width: parent.width; height: cropLayer.box.y; color: "#a0000000" }
        Rectangle { x: 0; y: cropLayer.box.y + cropLayer.box.height; width: parent.width; height: parent.height - y; color: "#a0000000" }
        Rectangle { x: 0; y: cropLayer.box.y; width: cropLayer.box.x; height: cropLayer.box.height; color: "#a0000000" }
        Rectangle { x: cropLayer.box.x + cropLayer.box.width; y: cropLayer.box.y; width: parent.width - x; height: cropLayer.box.height; color: "#a0000000" }
        Rectangle {
            x: cropLayer.box.x; y: cropLayer.box.y; width: cropLayer.box.width; height: cropLayer.box.height
            color: "transparent"; border.color: Theme.lime; border.width: 2
            // Rule-of-thirds guides.
            Repeater {
                model: 2
                delegate: Item {
                    required property int index
                    Rectangle { x: (index + 1) * parent.parent.width / 3; width: 1; height: parent.parent.height; color: Theme.alpha(Theme.lime, 0.4) }
                    Rectangle { y: (index + 1) * parent.parent.height / 3; height: 1; width: parent.parent.width; color: Theme.alpha(Theme.lime, 0.4) }
                }
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.CrossCursor
            property point start
            property bool moving: false
            property rect origin
            function constrain(x0, y0, x1, y1) {
                let w = Math.abs(x1 - x0), h = Math.abs(y1 - y0)
                if (stage.cropAspect > 0 && w > 0 && h > 0) {
                    if (w / h > stage.cropAspect) w = h * stage.cropAspect; else h = w / stage.cropAspect
                }
                const sx = x1 >= x0 ? 1 : -1, sy = y1 >= y0 ? 1 : -1
                let x = sx > 0 ? x0 : x0 - w, y = sy > 0 ? y0 : y0 - h
                // Keep inside the frame (shrinking keeps the aspect).
                if (x < 0) { w += x; x = 0 } if (y < 0) { h += y; y = 0 }
                if (x + w > width) w = width - x
                if (y + h > height) h = height - y
                if (stage.cropAspect > 0) { if (w / h > stage.cropAspect) w = h * stage.cropAspect; else h = w / stage.cropAspect }
                return Qt.rect(x, y, Math.max(0, w), Math.max(0, h))
            }
            onPressed: (mouse) => {
                start = Qt.point(mouse.x, mouse.y)
                const b = cropLayer.box
                moving = imageEditor.hasCrop && mouse.x > b.x && mouse.x < b.x + b.width && mouse.y > b.y && mouse.y < b.y + b.height
                origin = b
                cropLayer.live = b
                cropLayer.dragging = true
            }
            onPositionChanged: (mouse) => {
                if (!cropLayer.dragging) return
                if (moving) {
                    const x = Math.max(0, Math.min(width - origin.width, origin.x + mouse.x - start.x))
                    const y = Math.max(0, Math.min(height - origin.height, origin.y + mouse.y - start.y))
                    cropLayer.live = Qt.rect(x, y, origin.width, origin.height)
                } else {
                    cropLayer.live = constrain(start.x, start.y, Math.max(0, Math.min(width, mouse.x)), Math.max(0, Math.min(height, mouse.y)))
                }
            }
            onReleased: {
                const r = cropLayer.live
                cropLayer.dragging = false
                if (r.width / width > 0.015 && r.height / height > 0.015)
                    imageEditor.setCrop(r.x / width, r.y / height, r.width / width, r.height / height)
            }
        }
    }

    // ── 1:1 zoom ──
    Image {
        id: zoomImage
        visible: imageEditor.zoomed && status === Image.Ready
        anchors.centerIn: parent
        width: sourceSize.width; height: sourceSize.height
        source: imageEditor.zoomUrl
        asynchronous: false; cache: false
    }
    Text {
        visible: imageEditor.zoomed
        anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 10
        text: "1:1  ·  drag to pan  ·  Z or double-click to fit"; color: Theme.cyan; font.family: Theme.mono; font.pixelSize: 10
        style: Text.Outline; styleColor: Theme.bg
    }

    // Normal / zoom interaction (crop mode has its own layer above).
    MouseArea {
        anchors.fill: parent
        enabled: imageEditor.ready && !imageEditor.cropMode
        acceptedButtons: Qt.LeftButton
        cursorShape: imageEditor.zoomed ? (pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor) : Qt.ArrowCursor
        property point last
        z: -1
        onPressed: (mouse) => last = Qt.point(mouse.x, mouse.y)
        onPositionChanged: (mouse) => {
            if (!imageEditor.zoomed || !pressed) return
            imageEditor.panZoom(mouse.x - last.x, mouse.y - last.y)
            last = Qt.point(mouse.x, mouse.y)
        }
        onDoubleClicked: (mouse) => stage.toggleZoom(mouse.x, mouse.y)
    }

    Text {
        anchors.centerIn: parent; visible: preview.status !== Image.Ready && !imageEditor.zoomed
        text: imageEditor.status; color: Theme.muted; font.family: Theme.mono; font.pixelSize: 11
        wrapMode: Text.Wrap; width: parent.width - 40; horizontalAlignment: Text.AlignHCenter
    }
}
