#include "core/profile/ProfileStore.h"
#include "core/storage/Database.h"
#include "core/storage/LocalProtection.h"
#include "core/storage/ChatStore.h"
#include "core/tox/ToxService.h"
#include <QFile>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QJsonObject>
#include <QTimer>
#include <QtTest>

class FoundationTest : public QObject {
    Q_OBJECT
private slots:
    void profileAndMetadata();
    void malformedMetadata();
    void atomicSavedata();
    void migrations();
    void addressEncoding();
    void identityRestartAndStress();
    void windowsIdentityProtection();
    void encryptedHistoryIntegrity();
    void realMessagingAndRestart();
    void profileExclusiveLock();
    void externalClientInterop();
};

void FoundationTest::profileAndMetadata() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ProfileStore store(dir.path()); Profile first, second; QString error;
    QVERIFY2(store.open(first, error), qPrintable(error));
    QVERIFY(first.isNew);
    QVERIFY(!first.id.isNull());
    const QByteArray data("identity test data");
    QVERIFY2(ProfileStore::writeSavedata(first, data, error), qPrintable(error));
    QVERIFY2(store.open(second, error), qPrintable(error));
    QVERIFY(!second.isNew);
    QCOMPARE(second.id, first.id);
    QCOMPARE(second.name, first.name);
    QCOMPARE(second.created, first.created);
    QCOMPARE(second.directory, first.directory);
}

void FoundationTest::malformedMetadata() {
    QTemporaryDir dir; ProfileStore store(dir.path()); Profile profile; QString error;
    QVERIFY(store.open(profile, error));
    QVERIFY(ProfileStore::writeSavedata(profile, QByteArray("state"), error));
    QFile file(profile.directory + "/profile.json");
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write("{garbage"), qint64(8)); file.close();
    Profile reopened;
    QVERIFY(!store.open(reopened, error));
    QVERIFY(!error.isEmpty());
    QVERIFY(QFile::exists(profile.savedataPath()));
}

void FoundationTest::atomicSavedata() {
    QTemporaryDir dir; ProfileStore store(dir.path()); Profile profile; QString error;
    QVERIFY(store.open(profile, error));
    QVERIFY(ProfileStore::writeSavedata(profile, QByteArray("original"), error));
    QVERIFY(!ProfileStore::writeSavedata(profile, {}, error));
    QByteArray read;
    QVERIFY(ProfileStore::readSavedata(profile, read, error));
    QCOMPARE(read, QByteArray("original"));
}

void FoundationTest::migrations() {
    QTemporaryDir dir; Database db; QString error;
    QVERIFY2(db.open(dir.filePath("test.db"), error), qPrintable(error));
    QCOMPARE(db.schemaVersion(error), 1);
    db.close();
    QVERIFY2(db.open(dir.filePath("test.db"), error), qPrintable(error));
    QCOMPARE(db.schemaVersion(error), 1);
    db.close();
}

void FoundationTest::addressEncoding() {
    QCOMPARE(ProfileStore::encodeAddress(QByteArray::fromHex("00abff")), QStringLiteral("00ABFF"));
}

void FoundationTest::windowsIdentityProtection() {
#ifdef Q_OS_WIN
    const QByteArray secret("synthetic-test-identity-do-not-log");
    QByteArray a, b, plain; QString error;
    QVERIFY2(LocalProtection::protect(secret, a, error), qPrintable(error));
    QVERIFY(LocalProtection::protect(secret, b, error));
    QVERIFY(a != b);
    QVERIFY(!a.contains(secret));
    QVERIFY(LocalProtection::unprotect(a, plain, error));
    QCOMPARE(plain, secret);
    a[a.size() - 1] = char(a[a.size() - 1] ^ 1);
    QVERIFY(!LocalProtection::unprotect(a, plain, error));
    QVERIFY(plain.isEmpty());
    QVERIFY(!LocalProtection::unprotect(secret, plain, error));
    QTemporaryDir dir; ProfileStore store(dir.path()); Profile profile;
    QVERIFY(store.open(profile, error));
    QVERIFY(ProfileStore::writeSavedata(profile, secret, error));
    QFile file(profile.savedataPath()); QVERIFY(file.open(QIODevice::ReadOnly));
    const auto protectedFile = file.readAll(); file.close();
    QVERIFY(protectedFile.startsWith("PARLEYDP1"));
    QVERIFY(!protectedFile.contains(secret));
    QVERIFY(ProfileStore::readSavedata(profile, plain, error));
    QCOMPARE(plain, secret);
#else
    QSKIP("DPAPI is tested on the Windows CI runner");
#endif
}

void FoundationTest::identityRestartAndStress() {
    QTemporaryDir dir; ProfileStore store(dir.path()); Profile profile; QString error;
    QVERIFY(store.open(profile, error));
    QString firstId;
    for (int i = 0; i < 100; ++i) {
        Profile loaded;
        if (i == 0) loaded = profile;
        else QVERIFY2(store.open(loaded, error), qPrintable(error));
        ToxService service(loaded, nullptr, false);
        QSignalSpy ready(&service, &ToxService::identityReady);
        QSignalSpy failure(&service, &ToxService::failure);
        service.start();
        const bool received = ready.wait(10000);
        service.stop();
        QVERIFY2(received, "Tox identity was not ready");
        QVERIFY2(failure.isEmpty(), "Tox worker reported an error");
        const QString id = ready.first().first().toString();
        QCOMPARE(id.size(), 2 * 38); // TOX_ADDRESS_SIZE: public key + nospam + checksum
        if (i == 0) firstId = id;
        else QCOMPARE(id, firstId);
        QVERIFY(!service.isRunning());
    }
    QVERIFY(QFile::exists(profile.savedataPath()));
}

void FoundationTest::encryptedHistoryIntegrity() {
    QTemporaryDir dir; ProfileStore profiles(dir.path()); Profile profile; QString error;
    QVERIFY(profiles.open(profile, error)); ChatStore store(profile);
    const auto owner = QString("AB").repeated(32), peer = QString("CD").repeated(32);
    ChatState state{owner, QJsonArray{QJsonObject{{"key", peer}, {"text", "fixture request"}}},
        QJsonArray{QJsonObject{{"id", "fixture-message"}, {"key", peer}, {"text", "history-secret-marker"},
            {"outgoing", false}, {"at", 12345.0}, {"delivery", "received"}}}};
    QVERIFY2(store.save(state, error), qPrintable(error));
    ChatState loaded; QVERIFY2(store.load(owner, loaded, error), qPrintable(error));
    QCOMPARE(loaded.messages, state.messages); QCOMPARE(loaded.requests, state.requests);
    QVERIFY(!store.load(peer, loaded, error));
    QFile file(store.path()); QVERIFY(file.open(QIODevice::ReadOnly));
    auto cipher = file.readAll(); file.close(); QVERIFY(!cipher.contains("history-secret-marker"));
    cipher[cipher.size() - 1] = char(cipher.back() ^ 1);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); QCOMPARE(file.write(cipher), cipher.size()); file.close();
    QVERIFY(!store.load(owner, loaded, error));
    QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), cipher);
}

void FoundationTest::profileExclusiveLock() {
    QTemporaryDir dir; ProfileStore profiles(dir.path()); Profile profile; QString error;
    QVERIFY(profiles.open(profile, error));
    ToxService a(profile, nullptr, false); QSignalSpy ready(&a, &ToxService::identityReady);
    a.start(); QVERIFY(ready.wait(10000));
    ToxService b(profile, nullptr, false); QSignalSpy failure(&b, &ToxService::failure);
    b.start(); QTRY_VERIFY_WITH_TIMEOUT(!failure.isEmpty(), 10000); b.stop(); a.stop();
    QByteArray identity; QVERIFY(ProfileStore::readSavedata(profile, identity, error)); QVERIFY(!identity.isEmpty());
}


static bool containsText(const QVariantMap &snapshot, const QString &text, const QString &delivery = {}) {
    for (const auto &entry : snapshot.value("messages").toList()) {
        const auto message = entry.toMap();
        if (message.value("text").toString() == text && (delivery.isEmpty() || message.value("delivery").toString() == delivery)) return true;
    }
    return false;
}
static bool online(const QVariantMap &snapshot) {
    const auto contacts = snapshot.value("contacts").toList();
    return contacts.size() == 1 && contacts.first().toMap().value("online").toBool();
}
void FoundationTest::realMessagingAndRestart() {
    QTemporaryDir rootA, rootB; ProfileStore storeA(rootA.path()), storeB(rootB.path()); Profile pa, pb; QString error;
    QVERIFY(storeA.open(pa, error)); QVERIFY(storeB.open(pb, error));
    ToxService a(pa, nullptr, false), b(pb, nullptr, false);
    QSignalSpy idA(&a, &ToxService::identityReady), idB(&b, &ToxService::identityReady);
    QSignalSpy epA(&a, &ToxService::localEndpointReady), epB(&b, &ToxService::localEndpointReady);
    QVariantMap sa, sb; QObject snapshots;
    connect(&a, &ToxService::snapshotReady, &snapshots, [&](const QVariantMap &value) { sa = value; });
    connect(&b, &ToxService::snapshotReady, &snapshots, [&](const QVariantMap &value) { sb = value; });
    QSignalSpy fa(&a, &ToxService::failure), fb(&b, &ToxService::failure);
    QSignalSpy accepted(&a, &ToxService::messageAccepted);
    a.start(); b.start();
    QTRY_VERIFY_WITH_TIMEOUT(!epA.isEmpty() && !epB.isEmpty(), 15000);
    const auto addressA = idA.first().first().toString(), addressB = idB.first().first().toString();
    auto bootstrapPeers = [&] {
        a.bootstrapNode("127.0.0.1", epB.first()[0].toInt(), epB.first()[1].toString());
        b.bootstrapNode("127.0.0.1", epA.first()[0].toInt(), epA.first()[1].toString());
    };
    QTimer timer; connect(&timer, &QTimer::timeout, this, bootstrapPeers); timer.start(500); bootstrapPeers();
    a.addContact(addressB, "real socket fixture");
    QTRY_COMPARE_WITH_TIMEOUT(sb.value("requests").toList().size(), 1, 60000);
    QCOMPARE(sb.value("requests").toList().first().toMap().value("text").toString(), QString("real socket fixture"));
    b.acceptRequest(addressA.left(64));
    QTRY_VERIFY_WITH_TIMEOUT(online(sa) && online(sb), 30000);
    const QString greeting = QStringLiteral("Привет из Parley Windows 👋"), reply = QStringLiteral("Ответ через Tox ✓");
    a.sendMessage(addressB.left(64), greeting);
    QTRY_VERIFY_WITH_TIMEOUT(containsText(sb, greeting, "received"), 10000);
    QTRY_COMPARE_WITH_TIMEOUT(accepted.size(), 1, 10000);
    b.sendMessage(addressA.left(64), reply);
    QTRY_VERIFY_WITH_TIMEOUT(containsText(sa, reply, "received"), 10000);
    QTRY_VERIFY_WITH_TIMEOUT(containsText(sa, greeting, "delivered"), 10000);
    QVERIFY(fa.isEmpty()); QVERIFY(fb.isEmpty());
    timer.stop(); a.stop(); b.stop();
    Profile reopened; QVERIFY(storeA.open(reopened, error));
    ToxService restored(reopened, nullptr, false); QSignalSpy restoredId(&restored, &ToxService::identityReady);
    QSignalSpy restoredFailure(&restored, &ToxService::failure); QVariantMap restoredState; QObject restoredSnapshots;
    connect(&restored, &ToxService::snapshotReady, &restoredSnapshots, [&](const QVariantMap &value) { restoredState = value; });
    restored.start(); QTRY_VERIFY_WITH_TIMEOUT(containsText(restoredState, reply), 10000); restored.stop();
    QCOMPARE(restoredId.first().first().toString(), addressA);
    QCOMPARE(restoredState.value("contacts").toList().size(), 1);
    QVERIFY(containsText(restoredState, greeting, "delivered")); QVERIFY(restoredFailure.isEmpty());
    QFile history(ChatStore(reopened).path()); QVERIFY(history.open(QIODevice::ReadOnly));
    QVERIFY(!history.readAll().contains(greeting.toUtf8()));
}

void FoundationTest::externalClientInterop() {
    const auto peer = qEnvironmentVariable("PARLEY_INTEROP_PEER").trimmed();
    if (peer.isEmpty()) QSKIP("Explicit external peer required; not a substitute for the deterministic socket tests");
    QVERIFY2(peer.size() == 76, "External fixture must be a full Tox ID");
    QTemporaryDir root; ProfileStore profiles(root.path()); Profile profile; QString error;
    QVERIFY(profiles.open(profile, error)); QVERIFY(profile.isNew);
    ToxService service(profile); QVariantMap snapshot; QObject snapshots;
    connect(&service, &ToxService::snapshotReady, &snapshots, [&](const QVariantMap &value) { snapshot = value; });
    QSignalSpy ready(&service, &ToxService::identityReady), failure(&service, &ToxService::failure);
    service.start(); QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(), 15000);
    service.addContact(peer, "Parley Windows compatibility test");
    QTRY_VERIFY_WITH_TIMEOUT(online(snapshot), 120000);
    const auto outgoing = QStringLiteral("Parley Windows -> Toxic: UTF-8 ✓");
    const auto reply = QStringLiteral("Toxic -> Parley Windows: OK ✓");
    service.sendMessage(peer.left(64).toUpper(), outgoing);
    QTRY_VERIFY_WITH_TIMEOUT(containsText(snapshot, reply, "received"), 120000);
    QTRY_VERIFY_WITH_TIMEOUT(containsText(snapshot, outgoing, "delivered"), 10000);
    service.stop(); QVERIFY(failure.isEmpty());
    ChatState stored; QVERIFY(ChatStore(profile).load(ready.first().first().toString().left(64), stored, error));
    QCOMPARE(stored.messages.size(), 2);
}

QTEST_GUILESS_MAIN(FoundationTest)
#include "TestFoundation.moc"
