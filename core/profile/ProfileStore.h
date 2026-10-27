#pragma once
#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QUuid>

struct Profile {
    QUuid id;
    QString name;
    QDateTime created;
    QString directory;
    bool isNew = false;
    QString savedataPath() const {
#ifdef Q_OS_WIN
        return directory + QStringLiteral("/profile.identity");
#else
        return directory + QStringLiteral("/profile.tox");
#endif
    }
    QString databasePath() const { return directory + QStringLiteral("/hau.db"); }
};

class ProfileStore {
public:
    explicit ProfileStore(QString root);
    static QString defaultRoot();
    bool open(Profile &profile, QString &error) const;
    static bool readSavedata(const Profile &profile, QByteArray &data, QString &error);
    static bool writeSavedata(const Profile &profile, const QByteArray &data, QString &error);
    static QString encodeAddress(const QByteArray &address);
private:
    QString root_;
};
