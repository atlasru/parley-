#pragma once
#include "core/profile/ProfileStore.h"
#include <QThread>
#include <atomic>
#include <QMutex>
#include <QVariantMap>
#include <deque>

class ToxService final : public QThread {
    Q_OBJECT
public:
    explicit ToxService(Profile profile, QObject *parent = nullptr, bool enableBootstrap = true);
    ~ToxService() override;
    void stop();
    void addContact(const QString &address, const QString &request);
    void acceptRequest(const QString &key);
    void rejectRequest(const QString &key);
    void sendMessage(const QString &key, const QString &text);
    void bootstrapNode(const QString &host, int port, const QString &key);
signals:
    void identityReady(const QString &toxId);
    void networkStateChanged(int state); // 0 offline, 1 connecting, 2 online
    void failure(const QString &message);
    void operationFailed(const QString &message);
    void snapshotReady(const QVariantMap &snapshot);
    void localEndpointReady(int port, const QString &dhtKey); // Public diagnostics / real socket tests; never secret keys.
    void messageAccepted(const QString &key, const QString &text);
protected:
    void run() override;
private:
    Profile profile_;
    std::atomic_bool stopping_{false};
    bool enableBootstrap_;
    enum class Kind { Add, Accept, Reject, Send, Bootstrap };
    struct Command { Kind kind; QString target; QString text; int port = 0; };
    void enqueue(Command command);
    QMutex commandsMutex_;
    std::deque<Command> commands_;
};
