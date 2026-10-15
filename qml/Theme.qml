pragma Singleton
import QtQuick

// Ghostframe's Cybercore palette and type. A singleton (not an id), so the
// name can't be shadowed inside delegates the way `palette` was.
QtObject {
    readonly property color bg: "#080a0d"
    readonly property color panel: "#0d1218"
    readonly property color panel2: "#18212a"
    readonly property color panelHover: "#202c36"
    readonly property color line: "#29323b"
    readonly property color text: "#edf1f4"
    readonly property color muted: "#c0c8d0"
    readonly property color darkText: "#101318"
    readonly property color cyan: "#35f4ff"
    readonly property color pink: "#ff36a8"
    readonly property color lime: "#b4f33a"

    readonly property string mono: "JetBrains Mono"
    readonly property string sans: "Inter"

    function alpha(c, a) { return Qt.rgba(c.r, c.g, c.b, a) }
    // Cycles cyan → pink → lime, e.g. for folder rows.
    function accentAt(i) { return [cyan, pink, lime][i % 3] }
}
