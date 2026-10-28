#include "AppViewModel.h"
#include "core/profile/ProfileStore.h"
#include "core/storage/Database.h"
#include "core/tox/ToxService.h"
#include <QDateTime>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTextStream>
#include <memory>
#include <QCommandLineParser>
#include <QDir>
#include <QLockFile>

static void logMessage(QtMsgType type, const QMessageLogContext &ctx, const QString &message) {
    const char *level = type == QtDebugMsg ? "DEBUG" : type == QtInfoMsg ? "INFO" :
                        type == QtWarningMsg ? "WARN" : type == QtCriticalMsg ? "ERROR" : "FATAL";
    QTextStream(stderr) << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
                        << " " << level << " " << (ctx.category ? ctx.category : "hau")
                        << " " << message << Qt::endl;
    if (type == QtFatalMsg) abort();
}

int main(int argc, char *argv[]) {
    // The native Windows style rejects custom control backgrounds used by this UI.
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Parley"));
    QCoreApplication::setApplicationName(QStringLiteral("Parley"));
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({"data-dir", "Use a separate profile directory (e.g. disposable interoperability tests)", "directory"});
    parser.process(application);
    qInstallMessageHandler(logMessage);
    AppViewModel viewModel;
    Profile profile;
    QString error;
    Database database;
    std::unique_ptr<ToxService> tox;
    const auto root = parser.isSet("data-dir") ? parser.value("data-dir") : ProfileStore::defaultRoot();
    QDir().mkpath(root);
    QLockFile instance(root + "/app.lock"); instance.setStaleLockTime(0);
    ProfileStore profiles(root);
    if (!instance.tryLock(0)) {
        viewModel.setError(QStringLiteral("This profile directory is already in use."));
    } else if (!profiles.open(profile, error)) {
        viewModel.setError(QStringLiteral("Profile could not be opened. Check your application data folder."));
        qCritical() << "Profile error:" << error;
    } else {
        viewModel.setProfile(profile);
        if (!database.open(profile.databasePath(), error)) {
            viewModel.setError(QStringLiteral("Profile database could not be opened."));
            qCritical() << "Database error:" << error;
        } else {
            tox = std::make_unique<ToxService>(profile);
            viewModel.attach(tox.get());
            tox->start();
        }
    }
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appModel"), &viewModel);
    engine.loadFromModule("HAU", "Main");
    if (engine.rootObjects().isEmpty()) {
        if (tox) tox->stop();
        return 1;
    }
    const int result = application.exec();
    if (tox) tox->stop();
    database.close();
    return result;
}
