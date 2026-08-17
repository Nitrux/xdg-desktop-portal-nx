// SPDX-License-Identifier: BSD-3-Clause
#include <MauiKit4/Core/mauiapp.h>

#include <KLocalizedString>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDebug>
#include <QDir>
#include <QSurfaceFormat>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QProcess>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QUrl>
#include <utility>

class PortalResponseWatcher final : public QObject
{
    Q_OBJECT

public:
    PortalResponseWatcher(QString operation, QString path, QObject *parent = nullptr)
        : QObject(parent)
        , m_operation(std::move(operation))
        , m_path(std::move(path))
    {
    }

public slots:
    void response(uint response, const QVariantMap &results)
    {
        const auto outcome = response == 0U
            ? QStringLiteral("accepted")
            : response == 1U ? QStringLiteral("cancelled") : QStringLiteral("ended-other");

        qDebug().noquote() << QStringLiteral("Portal response [%1] code=%2 (%3) path=%4")
                                 .arg(m_operation)
                                 .arg(response)
                                 .arg(outcome, m_path)
                          << results;

        if (response == 2U) {
            const auto error = results.value(QStringLiteral("error")).toString();
            if (error.isEmpty()) {
                qWarning().noquote()
                    << QStringLiteral("Portal response [%1] ended with code 2. For FileChooser this usually means the broker could not complete the backend D-Bus call; nonzero backend details are not forwarded. Match this request path in the backend log: %2")
                           .arg(m_operation, m_path);
            } else {
                qWarning().noquote() << QStringLiteral("Portal response [%1] code 2 detail: %2")
                                            .arg(m_operation, error);
            }
        }

        deleteLater();
    }

private:
    QString m_operation;
    QString m_path;
};

class PortalTestController final : public QObject
{
    Q_OBJECT

public slots:
    void fileChooser()
    {
        auto message = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.portal.FileChooser"),
            QStringLiteral("OpenFile"));
        auto options = requestOptions();
        options.insert(QStringLiteral("current_folder"),
                       QByteArray(QDir::homePath().toUtf8() + QByteArray(1, '\0')));
        const auto call = message << QString{} << QStringLiteral("Test file chooser")
                                  << options;
        qDebug() << "Testing FileChooser; navigate in the dialog and press Open:";
        watch(QStringLiteral("FileChooser.OpenFile"),
              QDBusConnection::sessionBus().asyncCall(call));
    }

    void openUri()
    {
        qDebug() << "Testing xdg-open with URL:";
        launch(QStringLiteral("xdg-open"), {QStringLiteral("https://nxos.org")});
    }

    void openWithApplication()
    {
        const auto desktop = QStandardPaths::locate(
            QStandardPaths::ApplicationsLocation, QStringLiteral("org.maui.nota.desktop"));
        if (desktop.isEmpty()) {
            qWarning() << "No org.maui.nota.desktop found; edit the test source.";
            return;
        }
        qDebug() << "Testing desktop application launch:" << desktop;
        launch(QStringLiteral("gio"), {QStringLiteral("launch"), desktop});
    }

    void oauthAuthorize()
    {
        qDebug() << "OAuth step 1: opening authorization URL through OpenURI";
        auto message = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.portal.OpenURI"),
            QStringLiteral("OpenURI"));
        const auto call = message
            << QString{}
            << QStringLiteral("https://example.com/oauth/authorize?client_id=nx-portal-test&redirect_uri=nx-portal-test://oauth/callback")
            << requestOptions();
        watch(QStringLiteral("OpenURI OAuth authorization"),
              QDBusConnection::sessionBus().asyncCall(call));
    }

    void oauthCallback()
    {
        qDebug() << "OAuth step 2: sending redirect through OpenURI with ask=true";
        auto message = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            QStringLiteral("/org/freedesktop/portal/desktop"),
            QStringLiteral("org.freedesktop.portal.OpenURI"),
            QStringLiteral("OpenURI"));
        const auto call = message
            << QString{}
            << QStringLiteral("nx-portal-test://oauth/callback?code=test-code&state=test-state")
            << requestOptions({{QStringLiteral("ask"), true}});
        watch(QStringLiteral("OpenURI OAuth callback"),
              QDBusConnection::sessionBus().asyncCall(call));
    }

private:
    QVariantMap requestOptions(QVariantMap options = {})
    {
        options.insert(QStringLiteral("handle_token"),
                       QStringLiteral("nx_test_%1_%2")
                           .arg(++m_requestSerial)
                           .arg(QRandomGenerator::global()->generate(), 0, 16));
        return options;
    }

    static void launch(const QString &program, const QStringList &arguments)
    {
        if (!QProcess::startDetached(program, arguments))
            qWarning() << "Could not launch" << program << arguments;
    }

    void watch(const QString &operation, const QDBusPendingCall &call)
    {
        auto *watcher = new QDBusPendingCallWatcher(call);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher,
                         [this, watcher, operation] {
                             const auto reply = watcher->reply();
                             if (reply.type() == QDBusMessage::ErrorMessage) {
                                 qWarning() << "Portal method call failed:" << operation << reply.errorName()
                                            << reply.errorMessage();
                                 watcher->deleteLater();
                                 return;
                             }

                             const auto arguments = reply.arguments();
                             if (arguments.size() == 1
                                 && arguments.constFirst().canConvert<QDBusObjectPath>()) {
                                 watchRequest(operation, arguments.constFirst().value<QDBusObjectPath>());
                             } else {
                                 qDebug() << "Portal reply:" << arguments;
                             }
                             watcher->deleteLater();
                         });
    }

    void watchRequest(const QString &operation, const QDBusObjectPath &requestPath)
    {
        const auto path = requestPath.path();
        qDebug().noquote() << QStringLiteral("Portal request created [%1]: %2")
                                 .arg(operation, path);

        auto *responseWatcher = new PortalResponseWatcher(operation, path, this);
        const auto connected = QDBusConnection::sessionBus().connect(
            QStringLiteral("org.freedesktop.portal.Desktop"), path,
            QStringLiteral("org.freedesktop.portal.Request"),
            QStringLiteral("Response"), responseWatcher,
            SLOT(response(uint,QVariantMap)));
        if (!connected) {
            qWarning() << "Could not subscribe to portal response:" << operation << path;
            responseWatcher->deleteLater();
        }
    }

    quint64 m_requestSerial{0};
};

#include "main.moc"

int main(int argc, char **argv)
{
    QSurfaceFormat format;
    format.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);

    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("xdg-desktop-portal-nx-test"));
    QCoreApplication::setOrganizationName(QStringLiteral("Nitrux"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("nxos.org"));
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("xdg-desktop-portal-nx-test"));
    MauiApp::instance()->setIconName(QStringLiteral("system-file-manager"));

    QQmlApplicationEngine engine;
    PortalTestController controller;
    for (const auto &argument : app.arguments()) {
        if (argument.startsWith(QStringLiteral("nx-portal-test://")))
            qDebug() << "OAuth callback received by test application:" << QUrl(argument);
    }
    engine.rootContext()->setContextProperty(QStringLiteral("portalTest"), &controller);
    engine.load(QUrl(QStringLiteral("qrc:/nx/test/tests/TestWindow.qml")));
    return engine.rootObjects().isEmpty() ? 1 : app.exec();
}
