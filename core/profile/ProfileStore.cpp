#include "ProfileStore.h"
#include "core/storage/LocalProtection.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

ProfileStore::ProfileStore(QString root) : root_(std::move(root)) {}

QString ProfileStore::defaultRoot() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

bool ProfileStore::open(Profile &profile, QString &error) const {
    QDir root(root_);
    if (root_.isEmpty() || !root.mkpath(QStringLiteral("profiles"))) {
        error = QStringLiteral("Cannot create the application data directory"); return false;
    }
    QDir profiles(root.filePath(QStringLiteral("profiles")));
    const auto dirs = profiles.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    if (dirs.size() > 1) {
        error = QStringLiteral("Multiple profiles found; profile selection is not available yet"); return false;
    }
    if (dirs.isEmpty()) {
        profile.id = QUuid::createUuid();
        profile.name = QStringLiteral("Parley user");
        profile.created = QDateTime::currentDateTimeUtc();
        profile.isNew = true;
        const QString id = profile.id.toString(QUuid::WithoutBraces);
        profile.directory = profiles.filePath(id);
        if (!profiles.mkpath(id)) { error = QStringLiteral("Cannot create profile directory"); return false; }
        QJsonObject meta{{"version", 1}, {"uuid", id}, {"name", profile.name},
                         {"created", profile.created.toString(Qt::ISODateWithMs)}};
        QSaveFile file(profile.directory + QStringLiteral("/profile.json"));
        const QByteArray json = QJsonDocument(meta).toJson();
        if (!file.open(QIODevice::WriteOnly) || file.write(json) != json.size() || !file.commit()) {
            error = QStringLiteral("Cannot save profile metadata"); return false;
        }
        return true;
    }
    profile.directory = profiles.filePath(dirs.first());
    QFile file(profile.directory + QStringLiteral("/profile.json"));
    if (!file.open(QIODevice::ReadOnly)) { error = QStringLiteral("Profile metadata is missing"); return false; }
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    const auto meta = document.object();
    profile.id = QUuid(meta.value(QStringLiteral("uuid")).toString());
    profile.name = meta.value(QStringLiteral("name")).toString();
    profile.created = QDateTime::fromString(meta.value(QStringLiteral("created")).toString(), Qt::ISODateWithMs);
    if (parse.error != QJsonParseError::NoError || !document.isObject() ||
        meta.value(QStringLiteral("version")).toInt() != 1 || profile.id.isNull() ||
        profile.id.toString(QUuid::WithoutBraces) != dirs.first() ||
        profile.name.trimmed().isEmpty() || !profile.created.isValid()) {
        error = QStringLiteral("Profile metadata is invalid"); return false;
    }
    profile.isNew = false;
    if (!QFileInfo::exists(profile.savedataPath())) {
        error = QStringLiteral("Profile identity is missing; automatic replacement is disabled"); return false;
    }
    return true;
}

bool ProfileStore::readSavedata(const Profile &profile, QByteArray &data, QString &error) {
    QFile file(profile.savedataPath());
    if (!file.open(QIODevice::ReadOnly)) { error = QStringLiteral("Cannot read profile identity"); return false; }
    data = file.readAll();
    if (data.isEmpty() || file.error() != QFile::NoError) {
        error = QStringLiteral("Profile identity is empty or unreadable"); return false;
    }
#ifdef Q_OS_WIN
    QByteArray plain;
    if (!LocalProtection::unprotect(data, plain, error)) { data.clear(); return false; }
    data = plain;
#endif
    return true;
}

bool ProfileStore::writeSavedata(const Profile &profile, const QByteArray &data, QString &error) {
    if (data.isEmpty() || !QFileInfo(profile.directory).isDir()) {
        error = QStringLiteral("Refusing to save empty identity or invalid profile path"); return false;
    }
    QByteArray stored = data;
#ifdef Q_OS_WIN
    if (!LocalProtection::protect(data, stored, error)) return false;
#endif
    QSaveFile file(profile.savedataPath());
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) { error = QStringLiteral("Cannot open identity transaction"); return false; }
    if (file.write(stored) != stored.size() || !file.commit()) {
        error = QStringLiteral("Cannot commit profile identity"); return false;
    }
    return true;
}

QString ProfileStore::encodeAddress(const QByteArray &address) {
    return QString::fromLatin1(address.toHex().toUpper());
}
