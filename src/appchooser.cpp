// SPDX-License-Identifier: BSD-3-Clause
#include "appchooser.hpp"
#include "portalrequest.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QDebug>
#include <QMetaObject>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <algorithm>
#include <memory>
#include <ranges>
#include <utility>

namespace
{
constexpr uint Success = 0U;
constexpr uint Cancelled = 1U;
constexpr uint Failed = 2U;

QString desktopId(const QString &applicationsDirectory, const QString &file)
{
    auto relative = QDir(applicationsDirectory).relativeFilePath(file);
    relative.replace(QLatin1Char('/'), QLatin1Char('-'));
    if (relative.endsWith(QStringLiteral(".desktop")))
        relative.chop(8);
    return relative;
}

QString canonicalAppId(QString id)
{
    id = id.trimmed();
    if (id.endsWith(QStringLiteral(".desktop")))
        id.chop(8);
    return id;
}
}

struct AppChooser::Pending
{
    QSharedPointer<PortalRequest> request;
    QDBusMessage call;
    QVariantMap properties;
};

AppChooser::AppChooser(QObject &parent, QQmlEngine &engine, QDBusConnection connection)
    : QDBusAbstractAdaptor(&parent)
    , m_engine(engine)
    , m_connection(std::move(connection))
{
    setAutoRelaySignals(true);
}

QVariantList AppChooser::applications(const QStringList &choices)
{
    QVariantList result;
    QSet<QString> found;
    QStringList appIds;
    for (const auto &choice : choices) {
        const auto id = canonicalAppId(choice);
        if (!id.isEmpty() && !appIds.contains(id))
            appIds.append(id);
    }
    const auto locations = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);

    for (const auto &location : locations) {
        const auto appDirectory = QDir(location).filePath(QStringLiteral("applications"));
        QDirIterator iterator(appDirectory,
                              {QStringLiteral("*.desktop")},
                              QDir::Files,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const auto file = iterator.next();
            const auto id = desktopId(appDirectory, file);
            if (found.contains(id) || (!appIds.isEmpty() && !appIds.contains(id))) {
                continue;
            }

            QSettings desktop(file, QSettings::IniFormat);
            desktop.beginGroup(QStringLiteral("Desktop Entry"));
            if (desktop.value(QStringLiteral("Type")).toString() != QStringLiteral("Application")
                || desktop.value(QStringLiteral("Hidden"), false).toBool()
                || desktop.value(QStringLiteral("NoDisplay"), false).toBool()) {
                continue;
            }

            const auto name = desktop.value(QStringLiteral("Name")).toString();
            if (name.isEmpty()) {
                continue;
            }

            result.append(QVariantMap{
                {QStringLiteral("desktopId"), id},
                {QStringLiteral("name"), name},
                {QStringLiteral("comment"), desktop.value(QStringLiteral("Comment")).toString()},
                {QStringLiteral("icon"), desktop.value(QStringLiteral("Icon")).toString()},
                {QStringLiteral("exec"), desktop.value(QStringLiteral("Exec")).toString()}});
            found.insert(id);
        }
    }

    for (const auto &choice : appIds) {
        if (!found.contains(choice)) {
            result.append(QVariantMap{
                {QStringLiteral("desktopId"), choice},
                {QStringLiteral("name"), choice},
                {QStringLiteral("comment"), QString{}},
                {QStringLiteral("icon"), QStringLiteral("application-x-executable")},
                {QStringLiteral("exec"), choice}});
        }
    }

    std::ranges::sort(result, {}, [](const QVariant &entry) {
        return entry.toMap().value(QStringLiteral("name")).toString().toCaseFolded();
    });
    return result;
}

void AppChooser::ChooseApplication(const QDBusObjectPath &handle,
                                   const QString &appId,
                                   const QString &parentWindow,
                                   const QStringList &choices,
                                   const QVariantMap &options,
                                   const QDBusMessage &message)
{
    qInfo() << "AppChooser request" << handle.path() << "sender" << message.service()
            << "appId" << appId << "parent" << parentWindow
            << "choices" << choices << "options" << options;
    message.setDelayedReply(true);
    const auto path = handle.path();
    if (path.isEmpty() || m_pending.contains(path)) {
        qWarning() << "AppChooser rejected invalid or duplicate request" << path;
        m_connection.send(message.createErrorReply(
            QStringLiteral("org.freedesktop.portal.Error.Failed"),
            QStringLiteral("Invalid or duplicate request handle")));
        return;
    }

    auto pending = QSharedPointer<Pending>::create();
    pending->request = QSharedPointer<PortalRequest>::create(m_connection, path);
    pending->call = message;
    if (!pending->request->registerObject()) {
        const auto error = m_connection.lastError();
        qWarning() << "AppChooser could not export request" << path
                   << error.name() << error.message();
        m_connection.send(message.createErrorReply(
            QStringLiteral("org.freedesktop.portal.Error.Failed"),
            QStringLiteral("Could not export the request object")));
        return;
    }

    const auto subject = options.value(QStringLiteral("filename"),
        options.value(QStringLiteral("uri"),
            options.value(QStringLiteral("content_type")))).toString();
    QVariantMap properties{
        {QStringLiteral("bridge"), QVariant::fromValue(pending->request.data())},
        {QStringLiteral("dialogTitle"), QStringLiteral("Choose an Application")},
        {QStringLiteral("subject"), subject},
        {QStringLiteral("modalDialog"), options.value(QStringLiteral("modal"), true).toBool()},
        {QStringLiteral("applications"), applications(choices)},
        {QStringLiteral("lastChoice"), canonicalAppId(options.value(QStringLiteral("last_choice")).toString())}};

    pending->properties = properties;
    m_pending.insert(path, pending);

    connect(pending->request.data(), &PortalRequest::accepted, this,
            [this, path](const QVariant &value) {
                QTimer::singleShot(0, this, [this, path, value] {
                    const auto rawChoice = value.toString();
                    const auto choice = canonicalAppId(rawChoice);
                    qInfo() << "AppChooser selected" << rawChoice << "canonical ID" << choice;
                    finish(path,
                           choice.isEmpty() ? Cancelled : Success,
                           choice.isEmpty()
                               ? QVariantMap{}
                               : QVariantMap{{QStringLiteral("choice"), choice}});
                });
            });
    connect(pending->request.data(), &PortalRequest::cancelled, this,
            [this, path] {
                qInfo() << "AppChooser UI rejected or closed" << path;
                QTimer::singleShot(0, this, [this, path] {
                    finish(path, Cancelled, {});
                });
            });

    m_dialogQueue.append(path);
    qInfo() << "AppChooser queued" << path << "position" << m_dialogQueue.size()
            << "active" << m_activeDialogPath;
    showNextDialog();
}

void AppChooser::showNextDialog()
{
    if (!m_activeDialogPath.isEmpty()) {
        qInfo() << "AppChooser waiting; active dialog" << m_activeDialogPath
                << "queued" << m_dialogQueue.size();
        return;
    }

    while (!m_dialogQueue.isEmpty()) {
        const auto path = m_dialogQueue.takeFirst();
        const auto pending = m_pending.value(path);
        if (!pending)
            continue;

        m_activeDialogPath = path;
        qInfo() << "AppChooser activating" << path
                << "remaining queued" << m_dialogQueue.size();
        QTimer::singleShot(0, this, [this, path, pending] {
            if (m_activeDialogPath == path && m_pending.contains(path))
                showDialog(path, pending, pending->properties);
        });
        return;
    }
}

void AppChooser::showDialog(const QString &path,
                            const QSharedPointer<Pending> &pending,
                            const QVariantMap &properties,
                            int attempt)
{
    if (!m_pending.contains(path) || m_activeDialogPath != path) {
        qWarning() << "AppChooser skipped presentation for inactive request"
                   << path << "attempt" << attempt;
        return;
    }

    const auto present = [this, &path, &properties] {
        for (auto it = properties.cbegin(); it != properties.cend(); ++it) {
            if (!m_dialog->setProperty(it.key().toUtf8().constData(), it.value()))
                qWarning() << "AppChooser could not set persistent dialog property"
                           << it.key() << "for" << path;
        }
        qInfo() << "AppChooser presenting persistent QML window" << path << m_dialog.data();
        if (!QMetaObject::invokeMethod(m_dialog, "present")) {
            finish(path, Failed,
                   {{QStringLiteral("error"), QStringLiteral("Could not present AppChooser dialog")},
                    {QStringLiteral("nx_stage"), QStringLiteral("qml-presentation")}});
        }
    };

    if (m_dialog) {
        present();
        return;
    }

    qInfo() << "AppChooser creating persistent QML window" << path
            << "attempt" << attempt;
    QQmlComponent component(&m_engine,
                            QUrl(QStringLiteral("qrc:/nx/qml/src/qml/AppChooserDialog.qml")));
    if (component.status() != QQmlComponent::Ready) {
        qWarning() << "AppChooser QML is not ready" << path << component.errorString();
        if (attempt < 2) {
            QTimer::singleShot(100, this, [this, path, pending, properties, attempt] {
                showDialog(path, pending, properties, attempt + 1);
            });
            return;
        }
        finish(path, Failed,
               {{QStringLiteral("error"), component.errorString()},
                {QStringLiteral("nx_stage"), QStringLiteral("qml-component")}});
        return;
    }

    auto created = std::unique_ptr<QObject>(component.createWithInitialProperties(properties));
    if (!created) {
        qWarning() << "Could not create AppChooser QML" << path << component.errorString();
        if (attempt < 2) {
            QTimer::singleShot(100, this, [this, path, pending, properties, attempt] {
                showDialog(path, pending, properties, attempt + 1);
            });
            return;
        }
        finish(path, Failed,
               {{QStringLiteral("error"), component.errorString()},
                {QStringLiteral("nx_stage"), QStringLiteral("qml-object-creation")}});
        return;
    }

    QQmlEngine::setObjectOwnership(created.get(), QQmlEngine::CppOwnership);
    created->setParent(this);
    m_dialog = created.release();
    qInfo() << "AppChooser persistent QML window created" << m_dialog.data();
    present();
}

void AppChooser::UpdateChoices(const QDBusObjectPath &handle, const QStringList &choices)
{
    const auto path = handle.path();
    const auto pending = m_pending.value(path);
    if (!pending) {
        qWarning() << "AppChooser ignored UpdateChoices for unknown request" << path;
        return;
    }

    const auto updated = applications(choices);
    pending->properties.insert(QStringLiteral("applications"), updated);
    if (m_activeDialogPath == path && m_dialog)
        m_dialog->setProperty("applications", updated);
}

void AppChooser::finish(const QString &path, uint response, const QVariantMap &results)
{
    const auto pending = m_pending.value(path);
    if (!pending)
        return;

    const bool wasActive = m_activeDialogPath == path;
    m_dialogQueue.removeAll(path);
    if (wasActive) {
        m_activeDialogPath.clear();
        if (m_dialog && !QMetaObject::invokeMethod(m_dialog, "dismiss"))
            qWarning() << "AppChooser could not dismiss persistent QML window for" << path;
    }

    qInfo() << "AppChooser completing" << path << "response" << response
            << "results" << results;
    m_pending.remove(path);
    if (!m_connection.send(pending->call.createReply({response, results}))) {
        const auto error = m_connection.lastError();
        qWarning() << "AppChooser could not send reply for" << path
                   << error.name() << error.message();
    }

    if (wasActive)
        QTimer::singleShot(0, this, [this] { showNextDialog(); });
}
