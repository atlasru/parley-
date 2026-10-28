#pragma once
#include <QObject>
#include <QString>
#include "core/profile/ProfileStore.h"
#include <QVariantList>
class ToxService;

class AppViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString profileName READ profileName NOTIFY changed)
    Q_PROPERTY(QString toxId READ toxId NOTIFY changed)
    Q_PROPERTY(QString connectionStateText READ connectionStateText NOTIFY changed)
    Q_PROPERTY(int connectionState READ connectionState NOTIFY changed)
    Q_PROPERTY(QString initializationError READ initializationError NOTIFY changed)
    Q_PROPERTY(QVariantList contacts READ contacts NOTIFY changed)
    Q_PROPERTY(QVariantList requests READ requests NOTIFY changed)
    Q_PROPERTY(QVariantList messages READ messages NOTIFY changed)
    Q_PROPERTY(QString selectedKey READ selectedKey NOTIFY changed)
    Q_PROPERTY(QString selectedName READ selectedName NOTIFY changed)
    Q_PROPERTY(bool selectedOnline READ selectedOnline NOTIFY changed)
    Q_PROPERTY(bool sendBusy READ sendBusy NOTIFY changed)
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY changed)
    Q_PROPERTY(QString operationError READ operationError NOTIFY changed)
public:
    explicit AppViewModel(QObject *parent = nullptr);
    QString profileName() const { return name_; }
    QString toxId() const { return toxId_; }
    QString connectionStateText() const;
    int connectionState() const { return state_; }
    QString initializationError() const { return error_; }
    void setProfile(const Profile &profile);
    void setIdentity(const QString &id);
    void setState(int state);
    void setError(const QString &error);
    Q_INVOKABLE void copyToxId();
    void attach(ToxService *service);
    QVariantList contacts() const { return contacts_; }
    QVariantList requests() const { return requests_; }
    QVariantList messages() const;
    QString selectedKey() const { return selected_; }
    QString selectedName() const;
    bool selectedOnline() const;
    bool sendBusy() const { return sendBusy_; }
    bool darkTheme() const { return dark_; }
    QString operationError() const { return operationError_; }
    void setDarkTheme(bool dark);
    Q_INVOKABLE void selectContact(const QString &key);
    Q_INVOKABLE void addContact(const QString &id, const QString &request);
    Q_INVOKABLE void acceptRequest(const QString &key);
    Q_INVOKABLE void rejectRequest(const QString &key);
    Q_INVOKABLE void sendMessage(const QString &text);
    Q_INVOKABLE void clearOperationError();
signals:
    void changed();
    void messageAccepted(const QString &key, const QString &text);
private:
    QString name_;
    QString toxId_;
    QString error_;
    int state_ = 0;
    ToxService *service_ = nullptr;
    QVariantList contacts_, requests_, history_;
    QString selected_, operationError_;
    bool dark_ = true, sendBusy_ = false;
};
