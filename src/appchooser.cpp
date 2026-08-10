// SPDX-License-Identifier: BSD-3-Clause
#include "appchooser.hpp"
#include "portalrequest.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
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
    return relative.replace(QLatin1Char('/'), QLatin1Char('-'));
}
}

struct AppChooser::Pending
{
    QSharedPointer<PortalRequest> request;
    QPointer<QObject> dialog;
    QDBusMessage call;
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
            if (found.contains(id) || (!choices.isEmpty() && !choices.contains(id))) {
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

    for (const auto &choice : choices) {
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
    Q_UNUSED(appId)
    Q_UNUSED(parentWindow)
    message.setDelayedReply(true);
    const auto path = handle.path();
    if (path.isEmpty() || m_pending.contains(path)) {
        m_connection.send(message.createErrorReply(
            QStringLiteral("org.freedesktop.portal.Error.Failed"),
            QStringLiteral("Invalid or duplicate request handle")));
        return;
    }

    auto pending = QSharedPointer<Pending>::create();
    pending->request = QSharedPointer<PortalRequest>::create(m_connection, path);
    pending->call = message;
    if (!pending->request->registerObject()) {
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
        {QStringLiteral("lastChoice"), options.value(QStringLiteral("last_choice")).toString()}};

    QQmlComponent component(&m_engine, QUrl(QStringLiteral("qrc:/nx/qml/src/qml/AppChooserDialog.qml")));
    if (component.status() != QQmlComponent::Ready) {
        m_connection.send(message.createReply(
            {Failed, QVariantMap{{QStringLiteral("error"), component.errorString()}}}));
        return;
    }

    auto created = std::unique_ptr<QObject>(component.createWithInitialProperties(properties));
    if (!created) {
        m_connection.send(message.createReply(
            {Failed, QVariantMap{{QStringLiteral("error"), component.errorString()}}}));
        return;
    }
    created->setParent(pending->request.data());
    pending->dialog = created.get();
    created.release();
    m_pending.insert(path, pending);

    connect(pending->request.data(), &PortalRequest::accepted, this,
            [this, path](const QVariant &value) {
                QTimer::singleShot(0, this, [this, path, value] {
                    const auto choice = value.toString();
                    finish(path,
                           choice.isEmpty() ? Cancelled : Success,
                           choice.isEmpty()
                               ? QVariantMap{}
                               : QVariantMap{{QStringLiteral("choice"), choice}});
                });
            });
    connect(pending->request.data(), &PortalRequest::cancelled, this,
            [this, path] {
                QTimer::singleShot(0, this, [this, path] {
                    finish(path, Cancelled, {});
                });
            });
}

void AppChooser::UpdateChoices(const QDBusObjectPath &handle, const QStringList &choices)
{
    const auto pending = m_pending.value(handle.path());
    if (pending && pending->dialog) {
        pending->dialog->setProperty("applications", applications(choices));
    }
}

void AppChooser::finish(const QString &path, uint response, const QVariantMap &results)
{
    const auto pending = m_pending.take(path);
    if (!pending) {
        return;
    }
    m_connection.send(pending->call.createReply({response, results}));
}

