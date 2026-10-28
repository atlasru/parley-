import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HAU

ApplicationWindow {
    id: window
    visible: true
    width: 1080; height: 740
    minimumWidth: 760; minimumHeight: 560
    title: "Parley"
    color: Theme.background
    font.family: Qt.platform.os === "windows" ? "Segoe UI" : "sans-serif"
    font.pixelSize: 15
    palette.window: Theme.background
    palette.windowText: Theme.text
    palette.base: Theme.elevated
    palette.text: Theme.text
    palette.button: Theme.surface
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.placeholderText: Theme.muted
    property var drafts: ({})
    Binding { target: Theme; property: "dark"; value: appModel.darkTheme }
    function choose(key) {
        if (appModel.selectedKey) drafts[appModel.selectedKey] = composer.text
        appModel.selectContact(key)
        composer.text = drafts[key] || ""
    }
    function delivery(value) {
        return ({pending:"Отправляем",sent:"Отправлено",delivered:"Получено клиентом",failed:"Ошибка отправки",unknown:"Статус неизвестен"})[value] || ""
    }
    Connections {
        target: appModel
        function onMessageAccepted(key, text) {
            if (appModel.selectedKey === key && composer.text === text) composer.clear()
            if (drafts[key] === text) drafts[key] = ""
        }
    }
    header: ToolBar {
        background: Rectangle { color: Theme.background }
        contentItem: RowLayout {
            spacing: 14
            Label { text: "Parley"; font.pixelSize: 27; font.bold: true; leftPadding: 18; color: Theme.text }
            Rectangle { width: 8; height: 8; radius: 4; color: appModel.connectionState === 2 ? Theme.success : Theme.warning }
            Label { text: appModel.connectionStateText; color: Theme.muted; font.pixelSize: 13 }
            Item { Layout.fillWidth: true }
            ToolButton { text: "Мой профиль"; onClicked: profileDialog.open() }
            ToolButton { text: appModel.darkTheme ? "Светлая тема" : "Тёмная тема"; onClicked: appModel.darkTheme = !appModel.darkTheme }
            ToolButton { text: "+ Контакт"; enabled: appModel.connectionState !== 0; onClicked: addDialog.open() }
        }
    }
    ColumnLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            visible: appModel.initializationError.length > 0 || appModel.operationError.length > 0
            color: Theme.surface; Layout.fillWidth: true
            implicitHeight: errorRow.implicitHeight + 20
            RowLayout {
                id: errorRow; anchors.fill: parent; anchors.margins: 10
                Label {
                    text: appModel.initializationError || appModel.operationError
                    textFormat: Text.PlainText; wrapMode: Text.WordWrap
                    Layout.fillWidth: true; color: Theme.error
                }
                ToolButton { text: "Закрыть"; visible: !appModel.initializationError; onClicked: appModel.clearOperationError() }
            }
        }
        SplitView {
            Layout.fillWidth: true; Layout.fillHeight: true
            handle: Rectangle { implicitWidth: 1; color: Theme.surface }
            Rectangle {
                SplitView.preferredWidth: 310; SplitView.minimumWidth: 260; color: Theme.elevated
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 14; spacing: 12
                    Label { text: "Разговоры"; font.pixelSize: 20; font.bold: true; color: Theme.text }
                    Label { visible: appModel.requests.length > 0; text: "Заявки в контакты"; color: Theme.muted }
                    ListView {
                        id: requests; visible: count > 0; Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(contentHeight, 230); clip: true; spacing: 8
                        model: appModel.requests
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            required property var modelData
                            width: requests.width; implicitHeight: requestBody.implicitHeight + 24
                            color: Theme.surface; radius: 12
                            ColumnLayout {
                                id: requestBody; anchors.fill: parent; anchors.margins: 12; spacing: 6
                                Label { text: modelData.key.slice(0,16) + "…"; textFormat: Text.PlainText; color: Theme.text; font.bold: true }
                                Label { text: modelData.text; textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; color: Theme.text }
                                RowLayout {
                                    Button { text: "Принять"; enabled: appModel.connectionState !== 0; onClicked: appModel.acceptRequest(modelData.key) }
                                    Button { text: "Отклонить"; enabled: appModel.connectionState !== 0; onClicked: appModel.rejectRequest(modelData.key) }
                                }
                            }
                        }
                    }
                    ListView {
                        id: contacts; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 5
                        model: appModel.contacts
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            required property var modelData
                            width: contacts.width; height: 74; radius: 12
                            color: appModel.selectedKey === modelData.key ? Theme.surface : "transparent"
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 10; spacing: 12
                                Rectangle {
                                    width: 42; height: 42; radius: 21; color: Theme.surface
                                    Label { anchors.centerIn: parent; text: modelData.name.slice(0,1).toUpperCase(); textFormat: Text.PlainText; color: Theme.accent; font.bold: true; font.pixelSize: 20 }
                                }
                                ColumnLayout {
                                    spacing: 3; Layout.fillWidth: true
                                    Label { text: modelData.name; textFormat: Text.PlainText; elide: Text.ElideRight; Layout.fillWidth: true; color: Theme.text; font.bold: true }
                                    Label { text: modelData.online && appModel.connectionState !== 0 ? "В сети" : "Не в сети"; color: Theme.muted; font.pixelSize: 12 }
                                }
                            }
                            MouseArea { anchors.fill: parent; onClicked: window.choose(modelData.key) }
                        }
                        Label {
                            visible: contacts.count === 0; anchors.centerIn: parent
                            width: parent.width - 20; text: "Добавьте друга по Tox ID или поделитесь своим ID из профиля."
                            wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; color: Theme.muted
                        }
                    }
                    Label { text: "История хранится на этом устройстве"; color: Theme.muted; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                }
            }
            Rectangle {
                SplitView.fillWidth: true; color: Theme.background
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 20; spacing: 12
                    ColumnLayout {
                        visible: appModel.selectedKey.length > 0; spacing: 4
                        Label { text: appModel.selectedName; textFormat: Text.PlainText; font.pixelSize: 23; font.bold: true; color: Theme.text }
                        Label { text: appModel.selectedOnline ? "В сети" : "Не в сети · доставка доступна, когда вы оба онлайн"; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap }
                    }
                    ListView {
                        id: messages; Layout.fillWidth: true; Layout.fillHeight: true
                        model: appModel.messages; spacing: 10; clip: true
                        ScrollBar.vertical: ScrollBar {}
                        onCountChanged: Qt.callLater(function() { messages.positionViewAtEnd() })
                        delegate: Item {
                            required property var modelData
                            width: messages.width; height: bubble.height
                            TextMetrics { id: metrics; text: modelData.text; font.pixelSize: 16 }
                            Rectangle {
                                id: bubble
                                anchors.right: modelData.outgoing ? parent.right : undefined
                                anchors.left: modelData.outgoing ? undefined : parent.left
                                width: Math.min(Math.max(metrics.width + 28, 220), Math.min(560, parent.width * 0.88))
                                height: messageBody.implicitHeight + 24; radius: 16
                                color: modelData.outgoing ? Theme.outgoing : Theme.surface
                                Column {
                                    id: messageBody; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                    anchors.margins: 12; spacing: 6
                                    Text {
                                        width: parent.width; text: (modelData.action ? "* " : "") + modelData.text
                                        textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: Theme.text; font.pixelSize: 16
                                    }
                                    Text {
                                        width: parent.width
                                        text: Qt.formatDateTime(new Date(Number(modelData.at)), "HH:mm") + (modelData.outgoing ? " · " + window.delivery(modelData.delivery) : "")
                                        textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 11; horizontalAlignment: Text.AlignRight; wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                        Label { visible: !appModel.selectedKey; anchors.centerIn: parent; text: "Место для ваших разговоров"; color: Theme.muted; font.pixelSize: 23 }
                    }
                    RowLayout {
                        visible: appModel.selectedKey.length > 0; Layout.fillWidth: true; spacing: 12
                        TextArea {
                            id: composer; Layout.fillWidth: true; Layout.maximumHeight: 130
                            placeholderText: "Сообщение"; textFormat: TextEdit.PlainText; wrapMode: TextEdit.Wrap
                            selectByMouse: true; color: Theme.text; padding: 13
                            background: Rectangle { color: Theme.elevated; radius: 16; border.color: Theme.surface }
                            Keys.onPressed: function(event) {
                                if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && !(event.modifiers & Qt.ShiftModifier)) {
                                    event.accepted = true
                                    if (sendButton.enabled) appModel.sendMessage(composer.text)
                                }
                            }
                        }
                        Button {
                            id: sendButton; text: "Отправить"; Layout.alignment: Qt.AlignBottom
                            enabled: appModel.selectedOnline && !appModel.sendBusy && composer.text.trim().length > 0
                            onClicked: appModel.sendMessage(composer.text)
                        }
                    }
                }
            }
        }
    }
    Dialog {
        id: addDialog; title: "Новый контакт"; anchors.centerIn: parent; modal: true
        width: Math.min(520, window.width - 40); standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: appModel.addContact(address.text, requestText.text)
        ColumnLayout {
            anchors.fill: parent; spacing: 14
            Label { text: "Вставьте полный Tox ID. Сверьте его с другом по доверенному каналу."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            TextArea { id: address; Layout.fillWidth: true; placeholderText: "Tox ID · 76 символов"; wrapMode: TextEdit.WrapAnywhere; selectByMouse: true; textFormat: TextEdit.PlainText }
            TextField { id: requestText; Layout.fillWidth: true; placeholderText: "Сообщение в заявке" }
        }
    }
    Dialog {
        id: profileDialog; title: "Ваш профиль Parley"; anchors.centerIn: parent; modal: true
        width: Math.min(560, window.width - 40); standardButtons: Dialog.Close
        ColumnLayout {
            anchors.fill: parent; spacing: 14
            Label { text: appModel.profileName; textFormat: Text.PlainText; font.pixelSize: 20 }
            TextArea { text: appModel.toxId; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; textFormat: TextEdit.PlainText; Layout.fillWidth: true }
            Button { text: "Копировать Tox ID"; onClicked: appModel.copyToxId() }
            Label {
                text: "Tox не скрывает IP-адреса и все метаданные. Офлайн-доставка не гарантируется. Полного независимого аудита Parley/Tox нет."
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted
            }
        }
    }
}
