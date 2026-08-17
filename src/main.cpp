// SPDX-License-Identifier: BSD-3-Clause
#include "appchooser.hpp"
#include "filechooser.hpp"
#include "openuri.hpp"
#include "settings.hpp"

#include <MauiKit4/Core/mauiapp.h>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusError>
#include <QDBusServiceWatcher>
#include <QDebug>
#include <QGuiApplication>
#include <QSurfaceFormat>
#include <QQmlContext>
#include <QQmlEngine>

#include <KLocalizedContext>
#include <KLocalizedString>

namespace
{
constexpr auto ServiceName = "org.freedesktop.impl.portal.desktop.nx";
constexpr auto ObjectPath = "/org/freedesktop/portal/desktop";

class PortalObject final : public QObject, public QDBusContext
{
};
}

int main(int argc, char *argv[])
{
    QSurfaceFormat format;
    format.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);

    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("xdg-desktop-portal-nx"));
    QCoreApplication::setOrganizationName(QStringLiteral("Nitrux"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    KLocalizedString::setApplicationDomain("xdg-desktop-portal-nx");
    application.setQuitOnLastWindowClosed(false);

    MauiApp::instance()->setIconName(QStringLiteral("system-file-manager"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption replaceOption(
        QStringLiteral("replace"),
        QStringLiteral("Replace the running portal backend instance."));
    parser.addOption(replaceOption);
    parser.process(application);

    auto connection = QDBusConnection::sessionBus();
    if (!connection.isConnected()) {
        qCritical() << "Could not connect to the session D-Bus:"
                    << connection.lastError().message();
        return 1;
    }

    auto *connectionInterface = connection.interface();
    if (!connectionInterface) {
        qCritical() << "Could not access the session D-Bus connection interface.";
        return 1;
    }

    const auto queueOption = parser.isSet(replaceOption)
        ? QDBusConnectionInterface::ReplaceExistingService
        : QDBusConnectionInterface::DontQueueService;
    const auto serviceReply = connectionInterface->registerService(
        QString::fromLatin1(ServiceName), queueOption,
        QDBusConnectionInterface::AllowReplacement);
    if (!serviceReply.isValid()
        || serviceReply.value() != QDBusConnectionInterface::ServiceRegistered) {
        qCritical() << "Could not own" << ServiceName << ':'
                    << serviceReply.error().message();
        return 1;
    }

    QObject::connect(connectionInterface, &QDBusConnectionInterface::serviceUnregistered,
                     &application, [](const QString &service) {
                         if (service != QString::fromLatin1(ServiceName))
                             return;
                         qWarning() << "Portal backend service ownership was replaced; exiting cleanly.";
                         QCoreApplication::quit();
                     });

    QQmlEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
    PortalObject portalObject;

    FileChooser fileChooser(portalObject, engine, connection);
    AppChooser appChooser(portalObject, engine, connection);
    OpenUri openUri(portalObject, connection);
    Settings settings(portalObject, portalObject);

    if (!connection.registerObject(QString::fromLatin1(ObjectPath),
                                   &portalObject,
                                   QDBusConnection::ExportAdaptors)) {
        qCritical() << "Could not export" << ObjectPath << ':'
                    << connection.lastError().message();
        return 1;
    }

    QDBusServiceWatcher frontendWatcher(
        QStringLiteral("org.freedesktop.portal.Desktop"), connection,
        QDBusServiceWatcher::WatchForUnregistration, &application);
    QObject::connect(&frontendWatcher, &QDBusServiceWatcher::serviceUnregistered,
                     &application, [](const QString &service) {
                         qWarning() << "Portal frontend disappeared; exiting cleanly:" << service;
                         QCoreApplication::quit();
                     });

    return application.exec();
}

