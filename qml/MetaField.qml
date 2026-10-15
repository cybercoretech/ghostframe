import QtQuick
import QtQuick.Controls

TextField {
    id: field
    color: Theme.text; font.family: Theme.sans
    placeholderTextColor: Theme.muted
    background: Rectangle { color: Theme.bg; border.color: field.activeFocus ? Theme.pink : Theme.line; radius: 2 }
}
