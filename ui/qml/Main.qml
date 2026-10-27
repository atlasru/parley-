import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HAU

ApplicationWindow {
    id: window
    visible: true
    width: 760
    height: 560
    minimumWidth: 540
    minimumHeight: 470
    title: "Parley — Windows foundation"
    color: Theme.background

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 40
        spacing: 26
        RowLayout {
            Layout.fillWidth: true
            Text { text: "Parley"; color: Theme.text; font.pixelSize: 34; font.bold: true }
            Item { Layout.fillWidth: true }
            Text { text: "0.1.0  ·  EARLY ACCESS"; color: Theme.muted; font.pixelSize: 11; font.letterSpacing: 1.2 }
        }
        Text { text: "Your space on the Tox network"; color: Theme.muted; font.pixelSize: 15 }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 320
            radius: Theme.radius
            color: Theme.elevated
            border.color: Theme.surface
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 30
                spacing: Theme.spacing
                Text { text: "PROFILE"; color: Theme.muted; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.5 }
                Text { text: appModel.profileName || "—"; color: Theme.text; font.pixelSize: 24; font.bold: true }
                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.surface }
                Text { text: "NETWORK"; color: Theme.muted; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.5 }
                RowLayout {
                    spacing: 10
                    Rectangle {
                        width: 9; height: 9; radius: 5
                        color: appModel.connectionState === 2 ? Theme.success :
                               appModel.connectionState === 1 ? Theme.warning : Theme.error
                    }
                    Text { text: appModel.connectionStateText; color: Theme.text; font.pixelSize: 16 }
                }
                Text { text: "YOUR TOX ID"; color: Theme.muted; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.5 }
                Text {
                    Layout.fillWidth: true
                    text: appModel.toxId || "Creating your identity…"
                    color: Theme.text
                    font.pixelSize: 13
                    font.family: "monospace"
                    wrapMode: Text.WrapAnywhere
                    textFormat: Text.PlainText
                }
                Button {
                    text: "Copy Tox ID"
                    enabled: appModel.toxId.length > 0
                    onClicked: appModel.copyToxId()
                    background: Rectangle { radius: 10; color: parent.enabled ? Theme.accent : Theme.surface }
                    contentItem: Text { text: parent.text; color: Theme.background; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    Layout.preferredWidth: 140
                    Layout.preferredHeight: 38
                }
            }
        }
        Text {
            visible: appModel.initializationError.length > 0
            text: appModel.initializationError
            color: Theme.error
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Item { Layout.fillHeight: true }
        Text {
            text: "Windows foundation · Identity + DHT only. Chat is available in the Android MVP. Tox does not hide your IP."
            color: Theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }
    }
}
