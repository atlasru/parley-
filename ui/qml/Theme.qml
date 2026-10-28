pragma Singleton
import QtQuick
QtObject {
    property bool dark: true
    readonly property color background: dark ? "#10131a" : "#f8f9fd"
    readonly property color elevated: dark ? "#171c25" : "#ffffff"
    readonly property color surface: dark ? "#252d3a" : "#e9ecf3"
    readonly property color outgoing: dark ? "#304385" : "#e3e8ff"
    readonly property color text: dark ? "#f3f6fb" : "#20232d"
    readonly property color muted: dark ? "#a1adbf" : "#5d6474"
    readonly property color accent: dark ? "#8f9cff" : "#4361ee"
    readonly property color success: "#60d6ac"
    readonly property color warning: "#e8b568"
    readonly property color error: "#f0808d"
    readonly property int radius: 18
    readonly property int spacing: 16
}
