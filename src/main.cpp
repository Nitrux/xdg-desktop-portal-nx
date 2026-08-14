// SPDX-License-Identifier: BSD-3-Clause
#include "appchooser.hpp"
#include "filechooser.hpp"
#include "settings.hpp"

#include <MauiKit4/Core/mauiapp.h>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>
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

    QQmlEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
    QObject portalObject;
    auto connection = QDBusConnection::sessionBus();
    if (!connection.isConnected()) {
        qCritical() << "Could not connect to the session D-Bus:"
                    << connection.lastError().message();
        return 1;
    }

    FileChooser fileChooser(portalObject, engine, connection);
    AppChooser appChooser(portalObject, engine, connection);
    Settings settings(portalObject);

    if (!connection.registerObject(QString::fromLatin1(ObjectPath),
                                   &portalObject,
                                   QDBusConnection::ExportAdaptors)) {
        qCritical() << "Could not export" << ObjectPath << ':'
                    << connection.lastError().message();
        return 1;
    }

    if (!connection.registerService(QString::fromLatin1(ServiceName))) {
        qCritical() << "Could not own" << ServiceName << ':'
                    << connection.lastError().message();
        return 1;
    }

    return application.exec();
}

