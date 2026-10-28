#include "AppViewModel.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QSettings>
#include "core/tox/ToxService.h"

AppViewModel::AppViewModel(QObject *parent) : QObject(parent) { dark_ = QSettings().value("appearance/dark", true).toBool(); }
void AppViewModel::attach(ToxService *service) {
    service_ = service;
    connect(service, &ToxService::identityReady, this, &AppViewModel::setIdentity);
    connect(service, &ToxService::networkStateChanged, this, &AppViewModel::setState);
    connect(service, &ToxService::failure, this, &AppViewModel::setError);
    connect(service, &ToxService::operationFailed, this, [this](const QString &text) {
        operationError_ = text; sendBusy_ = false; emit changed();
    });
    connect(service, &ToxService::messageAccepted, this, [this](const QString &key, const QString &text) {
        sendBusy_ = false; emit messageAccepted(key, text); emit changed();
    });
    connect(service, &ToxService::snapshotReady, this, [this](const QVariantMap &snapshot) {
        contacts_ = snapshot.value("contacts").toList(); requests_ = snapshot.value("requests").toList();
        history_ = snapshot.value("messages").toList();
        if (selected_.isEmpty() && !contacts_.isEmpty()) selected_ = contacts_.first().toMap().value("key").toString();
        emit changed();
    });
}
QVariantList AppViewModel::messages() const {
    QVariantList result;
    for (const auto &entry : history_) if (entry.toMap().value("key").toString() == selected_) result.append(entry);
    return result;
}
QString AppViewModel::selectedName() const {
    for (const auto &c : contacts_) if (c.toMap().value("key").toString() == selected_) return c.toMap().value("name").toString();
    return {};
}
bool AppViewModel::selectedOnline() const {
    if (!state_) return false;
    for (const auto &c : contacts_) if (c.toMap().value("key").toString() == selected_) return c.toMap().value("online").toBool();
    return false;
}
void AppViewModel::setDarkTheme(bool dark) { dark_ = dark; QSettings().setValue("appearance/dark", dark); emit changed(); }
void AppViewModel::selectContact(const QString &key) { selected_ = key; emit changed(); }
void AppViewModel::addContact(const QString &id, const QString &request) { if (service_) service_->addContact(id, request); }
void AppViewModel::acceptRequest(const QString &key) { if (service_) service_->acceptRequest(key); }
void AppViewModel::rejectRequest(const QString &key) { if (service_) service_->rejectRequest(key); }
void AppViewModel::sendMessage(const QString &text) {
    if (!service_ || sendBusy_ || !state_) return;
    sendBusy_ = true; emit changed(); service_->sendMessage(selected_, text);
}
void AppViewModel::clearOperationError() { operationError_.clear(); emit changed(); }

QString AppViewModel::connectionStateText() const {
    if (!error_.isEmpty()) return QStringLiteral("Error");
    if (state_ == 2) return QStringLiteral("Connected");
    if (state_ == 1) return QStringLiteral("Connecting");
    return QStringLiteral("Disconnected");
}
void AppViewModel::setProfile(const Profile &profile) { name_ = profile.name; emit changed(); }
void AppViewModel::setIdentity(const QString &id) { toxId_ = id; emit changed(); }
void AppViewModel::setState(int state) { state_ = state; if (!state) sendBusy_ = false; emit changed(); }
void AppViewModel::setError(const QString &error) { error_ = error; state_ = 0; sendBusy_ = false; emit changed(); }
void AppViewModel::copyToxId() {
    if (!toxId_.isEmpty()) QGuiApplication::clipboard()->setText(toxId_);
}
