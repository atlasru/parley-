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

static void logMessage(QtMsgType type, const QMessageLogContext &ctx, const QString &message) {
    const char *level = type == QtDebugMsg ? "DEBUG" : type == QtInfoMsg ? "INFO" :
                        type == QtWarningMsg ? "WARN" : type == QtCriticalMsg ? "ERROR" : "FATAL";
    QTextStream(stderr) << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
                        << " " << level << " " << (ctx.category ? ctx.category : "hau")
                        << " " << message << Qt::endl;
    if (type == QtFatalMsg) abort();
}

int main(int argc, char *argv[]) {
    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Parley"));
    QCoreApplication::setApplicationName(QStringLiteral("Parley"));
    qInstallMessageHandler(logMessage);
    AppViewModel viewModel;
    Profile profile;
    QString error;
    Database database;
    std::unique_ptr<ToxService> tox;
    ProfileStore profiles(ProfileStore::defaultRoot());
    if (!profiles.open(profile, error)) {
        viewModel.setError(QStringLiteral("Profile could not be opened. Check your application data folder."));
        qCritical() << "Profile error:" << error;
    } else {
        viewModel.setProfile(profile);
        if (!database.open(profile.databasePath(), error)) {
            viewModel.setError(QStringLiteral("Profile database could not be opened."));
            qCritical() << "Database error:" << error;
        } else {
            tox = std::make_unique<ToxService>(profile);
            QObject::connect(tox.get(), &ToxService::identityReady, &viewModel, &AppViewModel::setIdentity);
            QObject::connect(tox.get(), &ToxService::networkStateChanged, &viewModel, &AppViewModel::setState);
            QObject::connect(tox.get(), &ToxService::failure, &viewModel, &AppViewModel::setError);
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
