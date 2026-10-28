// SPDX-License-Identifier: MIT
#include "ChatStore.h"
#include "LocalProtection.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QRegularExpression>
#include <QStringList>
#ifndef Q_OS_WIN
#include <sodium.h>
#endif

namespace {
constexpr qsizetype limit = 32 * 1024 * 1024;
bool keyValid(const QString &s) {
    static const QRegularExpression pattern(QStringLiteral("^[0-9A-F]{64}$"));
    return pattern.match(s).hasMatch();
}
#ifndef Q_OS_WIN
bool localKey(const QString &path, bool create, QByteArray &key, QString &error) {
    if (sodium_init() < 0) { error = QStringLiteral("Crypto initialization failed"); return false; }
    QFile existing(path);
    if (existing.exists()) {
        if (!existing.open(QIODevice::ReadOnly) || existing.size() != crypto_secretbox_KEYBYTES) {
            error = QStringLiteral("History key is unreadable; original history preserved"); return false;
        }
        key = existing.readAll(); return key.size() == crypto_secretbox_KEYBYTES;
    }
    if (!create) { error = QStringLiteral("History key is missing; original history preserved"); return false; }
    key.resize(crypto_secretbox_KEYBYTES); randombytes_buf(key.data(), key.size());
    QSaveFile file(path); file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
        file.write(key) != key.size() || !file.commit()) {
        key.fill('\0'); error = QStringLiteral("Cannot save history key"); return false;
    }
    return true;
}
#endif
bool validate(const QJsonObject &json, const QString &owner) {
    if (!keyValid(owner)) return false;
    if (json.value("version").toInt() != 1 || json.value("owner").toString() != owner ||
        !json.value("requests").isArray() || !json.value("messages").isArray()) return false;
    for (const auto &r : json.value("requests").toArray()) {
        const auto v = r.toObject();
        if (!keyValid(v.value("key").toString()) || !v.value("text").isString()) return false;
    }
    const QStringList deliveries{"pending", "sent", "delivered", "received", "failed", "unknown"};
    for (const auto &r : json.value("messages").toArray()) {
        const auto v = r.toObject();
        if (!keyValid(v.value("key").toString()) || v.value("id").toString().isEmpty() ||
            !v.value("text").isString() || !v.value("outgoing").isBool() || !v.value("at").isDouble() ||
            !deliveries.contains(v.value("delivery").toString())) return false;
    }
    return true;
}
}

bool ChatStore::load(const QString &owner, ChatState &state, QString &error) const {
    if (!keyValid(owner)) { error = QStringLiteral("Invalid history owner"); return false; }
    QFile file(path());
    if (!file.exists()) { state = {owner, {}, {}}; return true; }
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > limit + 4096) {
        error = QStringLiteral("History is unreadable; original file preserved"); return false;
    }
    auto sealed = file.readAll(); QByteArray plain;
#ifdef Q_OS_WIN
    if (!LocalProtection::unprotect(sealed, plain, error)) return false;
#else
    const QByteArray magic("PARLEYBOX1");
    if (!sealed.startsWith(magic) || sealed.size() < magic.size() + crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES) {
        error = QStringLiteral("Invalid encrypted history format"); return false;
    }
    QByteArray key;
    if (!localKey(path() + ".key", false, key, error)) return false;
    const auto *nonce = reinterpret_cast<const unsigned char *>(sealed.constData() + magic.size());
    const auto *cipher = nonce + crypto_secretbox_NONCEBYTES;
    const auto size = sealed.size() - magic.size() - crypto_secretbox_NONCEBYTES;
    plain.resize(size - crypto_secretbox_MACBYTES);
    const int result = crypto_secretbox_open_easy(reinterpret_cast<unsigned char *>(plain.data()), cipher,
        size, nonce, reinterpret_cast<const unsigned char *>(key.constData()));
    sodium_memzero(key.data(), key.size());
    if (result != 0) { plain.fill('\0'); error = QStringLiteral("History authentication failed; original file preserved"); return false; }
#endif
    QJsonParseError parse;
    const auto json = QJsonDocument::fromJson(plain, &parse).object(); plain.fill('\0');
    if (parse.error != QJsonParseError::NoError || !validate(json, owner)) {
        error = QStringLiteral("History is invalid or belongs to another identity"); return false;
    }
    state = {owner, json.value("requests").toArray(), json.value("messages").toArray()};
    return true;
}

bool ChatStore::save(const ChatState &state, QString &error) const {
    const QJsonObject json{{"version", 1}, {"owner", state.owner}, {"requests", state.requests}, {"messages", state.messages}};
    if (!validate(json, state.owner)) { error = QStringLiteral("Refusing to save invalid history"); return false; }
    auto plain = QJsonDocument(json).toJson(QJsonDocument::Compact); QByteArray sealed;
    if (plain.isEmpty() || plain.size() > limit) { plain.fill('\0'); error = QStringLiteral("History limit reached; nothing was deleted"); return false; }
#ifdef Q_OS_WIN
    const bool encrypted = LocalProtection::protect(plain, sealed, error); plain.fill('\0');
    if (!encrypted) return false;
#else
    QByteArray key;
    if (!localKey(path() + ".key", !QFile::exists(path()), key, error)) { plain.fill('\0'); return false; }
    QByteArray nonce(crypto_secretbox_NONCEBYTES, Qt::Uninitialized); randombytes_buf(nonce.data(), nonce.size());
    QByteArray cipher(plain.size() + crypto_secretbox_MACBYTES, Qt::Uninitialized);
    crypto_secretbox_easy(reinterpret_cast<unsigned char *>(cipher.data()), reinterpret_cast<const unsigned char *>(plain.constData()),
        plain.size(), reinterpret_cast<const unsigned char *>(nonce.constData()), reinterpret_cast<const unsigned char *>(key.constData()));
    sodium_memzero(key.data(), key.size()); plain.fill('\0'); sealed = QByteArray("PARLEYBOX1") + nonce + cipher;
#endif
    QSaveFile file(path()); file.setDirectWriteFallback(false);
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (!file.open(QIODevice::WriteOnly) || file.write(sealed) != sealed.size() || !file.commit()) {
        error = QStringLiteral("Cannot commit encrypted history"); return false;
    }
    return true;
}
