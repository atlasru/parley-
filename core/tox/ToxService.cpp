#include "ToxService.h"
#include "core/storage/ChatStore.h"
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QLockFile>
#include <QRegularExpression>
#include <QUuid>
#include <algorithm>
#include <array>
#include <memory>
#include <unordered_map>
#include <vector>
#include <toxcore/tox.h>
#include <toxcore/tox_options.h>

Q_LOGGING_CATEGORY(toxLog, "parley.tox")
namespace {
using ToxPtr = std::unique_ptr<Tox, decltype(&tox_kill)>;
using OptionsPtr = std::unique_ptr<Tox_Options, decltype(&tox_options_free)>;
bool validHex(const QString &s, int length) {
    static const QRegularExpression pattern(QStringLiteral("^[0-9A-Fa-f]+$"));
    return s.size() == length && pattern.match(s).hasMatch();
}
QString encode(const uint8_t *bytes, size_t size) {
    return QString::fromLatin1(QByteArray(reinterpret_cast<const char *>(bytes), size).toHex().toUpper());
}
QString friendKey(Tox *tox, uint32_t friendNo) {
    std::array<uint8_t, TOX_PUBLIC_KEY_SIZE> key{};
    return tox_friend_get_public_key(tox, friendNo, key.data(), nullptr) ? encode(key.data(), key.size()) : QString{};
}
uint32_t friendNumber(Tox *tox, const QString &key) {
    if (!validHex(key, 64)) return UINT32_MAX;
    const auto bytes = QByteArray::fromHex(key.toLatin1());
    return tox_friend_by_public_key(tox, reinterpret_cast<const uint8_t *>(bytes.constData()), nullptr);
}
bool saveIdentity(Tox *tox, const Profile &profile, QString &error) {
    const auto length = tox_get_savedata_size(tox);
    if (!length || length > 32 * 1024 * 1024) { error = QStringLiteral("Invalid identity size"); return false; }
    QByteArray data(length, Qt::Uninitialized);
    tox_get_savedata(tox, reinterpret_cast<uint8_t *>(data.data()));
    const bool ok = ProfileStore::writeSavedata(profile, data, error); data.fill('\0'); return ok;
}
struct Session {
    ToxService *service; Tox *tox; Profile profile; ChatStore store; ChatState state;
    bool dirty = false;
    std::unordered_map<uint64_t, QString> receipts;
    bool save() {
        QString error;
        if (saveIdentity(tox, profile, error) && store.save(state, error)) return true;
        qCWarning(toxLog) << "storage_failed";
        emit service->failure(QStringLiteral("Cannot save protected data. Connection stopped; previous files preserved."));
        return false;
    }
    void publish() {
        std::vector<uint32_t> numbers(tox_self_get_friend_list_size(tox));
        tox_self_get_friend_list(tox, numbers.data()); QVariantList contacts;
        for (const auto f : numbers) {
            const auto key = friendKey(tox, f); Tox_Err_Friend_Query error;
            const auto length = tox_friend_get_name_size(tox, f, &error); QByteArray name;
            if (error == TOX_ERR_FRIEND_QUERY_OK && length > 0 && length <= TOX_MAX_NAME_LENGTH) {
                name.resize(length); tox_friend_get_name(tox, f, reinterpret_cast<uint8_t *>(name.data()), nullptr);
            }
            contacts.append(QVariantMap{{"key", key}, {"name", name.isEmpty() ? key.left(12) : QString::fromUtf8(name)},
                {"online", tox_friend_get_connection_status(tox, f, nullptr) != TOX_CONNECTION_NONE}});
        }
        emit service->snapshotReady({{"contacts", contacts}, {"requests", state.requests.toVariantList()}, {"messages", state.messages.toVariantList()}});
        dirty = false;
    }
    void message(const QString &key, const QString &text, bool outgoing, const QString &delivery, bool action = false) {
        state.messages.append(QJsonObject{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"key", key},
            {"text", text}, {"outgoing", outgoing}, {"delivery", delivery}, {"at", double(QDateTime::currentMSecsSinceEpoch())}, {"action", action}});
        dirty = true;
    }
};
void onRequest(Tox *, const uint8_t *key, const uint8_t *text, size_t length, void *user) {
    auto &s = *static_cast<Session *>(user); const auto id = encode(key, TOX_PUBLIC_KEY_SIZE);
    if (s.state.requests.size() >= 200 || friendNumber(s.tox, id) != UINT32_MAX) return;
    for (const auto &r : s.state.requests) if (r.toObject().value("key").toString() == id) return;
    s.state.requests.append(QJsonObject{{"key", id}, {"text", QString::fromUtf8(reinterpret_cast<const char *>(text), length)}}); s.dirty = true;
}
void onMessage(Tox *, uint32_t f, Tox_Message_Type type, const uint8_t *text, size_t length, void *user) {
    auto &s = *static_cast<Session *>(user);
    s.message(friendKey(s.tox, f), QString::fromUtf8(reinterpret_cast<const char *>(text), length), false, "received", type == TOX_MESSAGE_TYPE_ACTION);
}
void onReceipt(Tox *, uint32_t f, uint32_t receipt, void *user) {
    auto &s = *static_cast<Session *>(user); const auto found = s.receipts.find((uint64_t(f) << 32) | receipt);
    if (found == s.receipts.end()) return;
    for (int i = 0; i < s.state.messages.size(); ++i) {
        auto message = s.state.messages[i].toObject();
        if (message.value("id").toString() == found->second) {
            message.insert("delivery", "delivered"); s.state.messages.replace(i, message); s.dirty = true; break;
        }
    }
    s.receipts.erase(found);
}
void onConnection(Tox *, uint32_t, Tox_Connection, void *u) { static_cast<Session *>(u)->dirty = true; }
void onName(Tox *, uint32_t, const uint8_t *, size_t, void *u) { static_cast<Session *>(u)->dirty = true; }
void bootstrap(Tox *tox, const QString &host, int port, const QString &key, int tcpPort) {
    const auto bytes = QByteArray::fromHex(key.toLatin1()); const auto address = host.toUtf8();
    tox_bootstrap(tox, address.constData(), port, reinterpret_cast<const uint8_t *>(bytes.constData()), nullptr);
    if (tcpPort) tox_add_tcp_relay(tox, address.constData(), tcpPort, reinterpret_cast<const uint8_t *>(bytes.constData()), nullptr);
}
void bootstrapDefaults(Tox *tox) {
    bootstrap(tox, "144.217.167.73", 33445, "7E5668E0EE09E19F320AD47902419331FFEE147BB3606769CFBE921A2A2FD34C", 3389);
    bootstrap(tox, "205.185.115.131", 53, "3091C6BEB2A993F1C6300C16549FABA67098FF3D62C6D253828B531470B53D68", 443);
    qCInfo(toxLog) << "bootstrap_attempt";
}
}

ToxService::ToxService(Profile profile, QObject *parent, bool enableBootstrap)
    : QThread(parent), profile_(std::move(profile)), enableBootstrap_(enableBootstrap) {}
ToxService::~ToxService() { stop(); }
void ToxService::stop() { stopping_.store(true); if (isRunning() && QThread::currentThread() != this) wait(); }
void ToxService::enqueue(Command command) {
    if (!isRunning() || stopping_.load()) { emit operationFailed(QStringLiteral("Connection is not running")); return; }
    QMutexLocker lock(&commandsMutex_); commands_.push_back(std::move(command));
}
void ToxService::addContact(const QString &address, const QString &request) { enqueue({Kind::Add, address, request}); }
void ToxService::acceptRequest(const QString &key) { enqueue({Kind::Accept, key, {}}); }
void ToxService::rejectRequest(const QString &key) { enqueue({Kind::Reject, key, {}}); }
void ToxService::sendMessage(const QString &key, const QString &text) { enqueue({Kind::Send, key, text}); }
void ToxService::bootstrapNode(const QString &host, int port, const QString &key) { enqueue({Kind::Bootstrap, host, key, port}); }

void ToxService::run() {
    QLockFile lock(profile_.directory + "/profile.lock"); lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) { emit failure(QStringLiteral("This profile is already in use")); return; }
    QByteArray saved; QString error;
    if (!profile_.isNew && !ProfileStore::readSavedata(profile_, saved, error)) {
        emit failure(QStringLiteral("Cannot read existing identity. Original data preserved.")); return;
    }
    OptionsPtr options(tox_options_new(nullptr), tox_options_free);
    if (!options) { emit failure(QStringLiteral("Cannot initialize Tox options")); return; }
    tox_options_set_ipv6_enabled(options.get(), enableBootstrap_);
    if (!saved.isEmpty()) {
        tox_options_set_savedata_type(options.get(), TOX_SAVEDATA_TYPE_TOX_SAVE);
        if (!tox_options_set_savedata_data(options.get(), reinterpret_cast<const uint8_t *>(saved.constData()), saved.size())) {
            saved.fill('\0'); emit failure(QStringLiteral("Cannot load identity")); return;
        }
    }
    Tox_Err_New newError; ToxPtr tox(tox_new(options.get(), &newError), tox_kill); saved.fill('\0'); options.reset();
    if (!tox || newError != TOX_ERR_NEW_OK) { emit failure(QStringLiteral("Invalid identity or unavailable networking. Original data preserved.")); return; }
    const auto name = profile_.name.toUtf8();
    tox_self_set_name(tox.get(), reinterpret_cast<const uint8_t *>(name.constData()), name.size(), nullptr);
    std::array<uint8_t, TOX_ADDRESS_SIZE> address{}; tox_self_get_address(tox.get(), address.data()); const auto id = encode(address.data(), address.size());
    Session session{this, tox.get(), profile_, ChatStore(profile_), {}, false, {}};
    if (!session.store.load(id.left(64), session.state, error)) { emit failure(QStringLiteral("Cannot unlock history. Original files preserved.")); return; }
    for (int i = 0; i < session.state.messages.size(); ++i) {
        auto m = session.state.messages[i].toObject(); const auto delivery = m.value("delivery").toString();
        if (delivery == "pending" || delivery == "sent") { m.insert("delivery", "unknown"); session.state.messages.replace(i, m); }
    }
    QJsonArray pending;
    for (const auto &r : session.state.requests) if (friendNumber(tox.get(), r.toObject().value("key").toString()) == UINT32_MAX) pending.append(r);
    session.state.requests = pending;
    if (!session.save()) return;
    tox_callback_friend_request(tox.get(), onRequest); tox_callback_friend_message(tox.get(), onMessage);
    tox_callback_friend_read_receipt(tox.get(), onReceipt); tox_callback_friend_connection_status(tox.get(), onConnection); tox_callback_friend_name(tox.get(), onName);
    emit identityReady(id); emit networkStateChanged(1); session.publish();
    std::array<uint8_t, TOX_PUBLIC_KEY_SIZE> dht{}; tox_self_get_dht_id(tox.get(), dht.data());
    emit localEndpointReady(tox_self_get_udp_port(tox.get(), nullptr), encode(dht.data(), dht.size()));
    QElapsedTimer clock; clock.start(); qint64 nextSave = 30000, nextBootstrap = 0, nextPublish = 1000; int lastState = 1; bool healthy = true;
    while (!stopping_.load() && healthy) {
        std::deque<Command> commands; { QMutexLocker lock(&commandsMutex_); commands.swap(commands_); }
        for (const auto &command : commands) {
            if (stopping_.load()) break;
            bool changed = false;
            if (command.kind == Kind::Bootstrap) {
                if (command.target.isEmpty() || command.port < 1 || command.port > 65535 || !validHex(command.text, 64)) emit operationFailed(QStringLiteral("Invalid bootstrap endpoint"));
                else bootstrap(tox.get(), command.target, command.port, command.text, 0);
            } else if (command.kind == Kind::Add) {
                auto target = command.target.trimmed(); if (target.startsWith("tox:", Qt::CaseInsensitive)) target.remove(0, 4);
                const auto request = (command.text.isEmpty() ? QStringLiteral("Hello from Parley") : command.text).toUtf8();
                if (!validHex(target, 76) || request.size() > TOX_MAX_FRIEND_REQUEST_LENGTH) { emit operationFailed(QStringLiteral("Use a complete 76-character Tox ID and a short request")); continue; }
                const auto bytes = QByteArray::fromHex(target.toLatin1()); Tox_Err_Friend_Add result;
                tox_friend_add(tox.get(), reinterpret_cast<const uint8_t *>(bytes.constData()), reinterpret_cast<const uint8_t *>(request.constData()), request.size(), &result);
                if (result != TOX_ERR_FRIEND_ADD_OK) emit operationFailed(QStringLiteral("Contact could not be added (check ID checksum, duplicate or own ID)"));
                else changed = true;
            } else if (command.kind == Kind::Accept || command.kind == Kind::Reject) {
                if (!validHex(command.target, 64)) { emit operationFailed(QStringLiteral("Invalid public key")); continue; }
                if (command.kind == Kind::Accept && friendNumber(tox.get(), command.target) == UINT32_MAX) {
                    const auto bytes = QByteArray::fromHex(command.target.toLatin1()); Tox_Err_Friend_Add result;
                    tox_friend_add_norequest(tox.get(), reinterpret_cast<const uint8_t *>(bytes.constData()), &result);
                    if (result != TOX_ERR_FRIEND_ADD_OK) { emit operationFailed(QStringLiteral("Cannot accept request")); continue; }
                }
                QJsonArray remaining; for (const auto &r : session.state.requests) if (r.toObject().value("key").toString() != command.target) remaining.append(r);
                session.state.requests = remaining; changed = true;
            } else if (command.kind == Kind::Send) {
                const auto text = command.text.toUtf8(); const auto f = friendNumber(tox.get(), command.target);
                if (command.text.trimmed().isEmpty() || text.size() > TOX_MAX_MESSAGE_LENGTH) { emit operationFailed(QStringLiteral("Message must contain 1-1372 UTF-8 bytes")); continue; }
                if (f == UINT32_MAX || tox_friend_get_connection_status(tox.get(), f, nullptr) == TOX_CONNECTION_NONE) { emit operationFailed(QStringLiteral("Contact is offline. There is no offline delivery queue.")); continue; }
                session.message(command.target, command.text, true, "pending"); if (!session.save()) { healthy = false; break; }
                auto m = session.state.messages.last().toObject(); Tox_Err_Friend_Send_Message result;
                const auto receipt = tox_friend_send_message(tox.get(), f, TOX_MESSAGE_TYPE_NORMAL, reinterpret_cast<const uint8_t *>(text.constData()), text.size(), &result);
                m.insert("delivery", result == TOX_ERR_FRIEND_SEND_MESSAGE_OK ? "sent" : "failed"); session.state.messages.replace(session.state.messages.size() - 1, m);
                if (result == TOX_ERR_FRIEND_SEND_MESSAGE_OK) session.receipts[(uint64_t(f) << 32) | receipt] = m.value("id").toString();
                if (!session.save()) { healthy = false; break; } session.publish();
                if (result == TOX_ERR_FRIEND_SEND_MESSAGE_OK) emit messageAccepted(command.target, command.text);
                else emit operationFailed(QStringLiteral("Native send failed. Message was not automatically retried."));
            }
            if (changed) { if (!session.save()) { healthy = false; break; } session.publish(); }
        }
        if (!healthy) break;
        tox_iterate(tox.get(), &session);
        const auto now = clock.elapsed(); const int state = tox_self_get_connection_status(tox.get()) == TOX_CONNECTION_NONE ? 1 : 2;
        if (state != lastState) { lastState = state; emit networkStateChanged(state); }
        if (enableBootstrap_ && now >= nextBootstrap && state == 1) { bootstrapDefaults(tox.get()); nextBootstrap = now + 60000; }
        if (session.dirty || now >= nextSave) { healthy = session.save(); nextSave = now + 30000; }
        if (healthy && (session.dirty || now >= nextPublish)) { session.publish(); nextPublish = now + 1000; }
        QThread::msleep(std::clamp(tox_iteration_interval(tox.get()), uint32_t(10), uint32_t(100)));
    }
    if (healthy) session.save();
    emit networkStateChanged(0); qCInfo(toxLog) << "engine_stopped";
}
