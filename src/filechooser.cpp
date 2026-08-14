// SPDX-License-Identifier: BSD-3-Clause
#include "filechooser.hpp"
#include "portalrequest.hpp"

#include <QByteArray>
#include <QDBusArgument>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTimer>
#include <QUrl>
#include <memory>
#include <utility>

namespace
{
constexpr uint Success = 0U;
constexpr uint Cancelled = 1U;
constexpr uint Failed = 2U;

QString pathOption(const QVariantMap &options, const QString &name)
{
    auto bytes = options.value(name).toByteArray();
    if (bytes.endsWith('\0')) {
        bytes.chop(1);
    }
    return QFile::decodeName(bytes);
}

QStringList decodeNameFilters(const QVariant &value)
{
    if (!value.canConvert<QDBusArgument>()) {
        return {};
    }

    QStringList filters;
    const auto argument = value.value<QDBusArgument>();
    argument.beginArray();
    while (!argument.atEnd()) {
        QString label;
        argument.beginStructure();
        argument >> label;
        argument.beginArray();
        while (!argument.atEnd()) {
            uint kind = 0U;
            QString pattern;
            argument.beginStructure();
            argument >> kind >> pattern;
            argument.endStructure();

            if (kind == 0U) {
                filters.append(pattern);
            } else if (kind == 1U) {
                const QMimeDatabase database;
                if (pattern.endsWith(QStringLiteral("/*"))) {
                    const auto prefix = pattern.first(pattern.size() - 1);
                    for (const auto &mime : database.allMimeTypes()) {
                        if (mime.name().startsWith(prefix)) {
                            filters.append(mime.globPatterns());
                        }
                    }
                } else {
                    filters.append(database.mimeTypeForName(pattern).globPatterns());
                }
            }
        }
        argument.endArray();
        argument.endStructure();
    }
    argument.endArray();
    filters.removeDuplicates();
    return filters;
}

QStringList selectedUris(const QVariant &value, bool directory)
{
    QStringList uris;
    const auto values = value.toList();
    for (const auto &entry : values) {
        QUrl url;
        if (entry.canConvert<QUrl>()) {
            url = entry.toUrl();
        } else {
            url = QUrl::fromUserInput(entry.toString());
        }

        if (!url.isLocalFile()) {
            continue;
        }

        const QFileInfo info(url.toLocalFile());
        if (directory != info.isDir()) {
            continue;
        }
        uris.append(QUrl::fromLocalFile(info.absoluteFilePath()).toString(QUrl::FullyEncoded));
    }
    uris.removeDuplicates();
    return uris;
}
}

struct FileChooser::Pending
{
    QSharedPointer<PortalRequest> request;
    QPointer<QObject> dialog;
    QDBusMessage call;
    bool directory{false};
    bool multiple{false};
    bool save{false};
};

FileChooser::FileChooser(QObject &parent, QQmlEngine &engine, QDBusConnection connection)
    : QDBusAbstractAdaptor(&parent)
    , m_engine(engine)
    , m_connection(std::move(connection))
{
    setAutoRelaySignals(true);
}

void FileChooser::OpenFile(const QDBusObjectPath &handle,
                           const QString &appId,
                           const QString &parentWindow,
                           const QString &title,
                           const QVariantMap &options,
                           const QDBusMessage &message)
{
    Q_UNUSED(appId)
    Q_UNUSED(parentWindow)
    begin(handle, title, options, message, false);
}

void FileChooser::SaveFile(const QDBusObjectPath &handle,
                           const QString &appId,
                           const QString &parentWindow,
                           const QString &title,
                           const QVariantMap &options,
                           const QDBusMessage &message)
{
    Q_UNUSED(appId)
    Q_UNUSED(parentWindow)
    begin(handle, title, options, message, true);
}

void FileChooser::begin(const QDBusObjectPath &handle,
                        const QString &title,
                        const QVariantMap &options,
                        const QDBusMessage &message,
                        bool save)
{
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
    pending->directory = !save && options.value(QStringLiteral("directory"), false).toBool();
    pending->multiple = !save && options.value(QStringLiteral("multiple"), false).toBool();
    pending->save = save;

    if (!pending->request->registerObject()) {
        m_connection.send(message.createErrorReply(
            QStringLiteral("org.freedesktop.portal.Error.Failed"),
            QStringLiteral("Could not export the request object")));
        return;
    }

    QVariantMap properties{
        {QStringLiteral("bridge"), QVariant::fromValue(pending->request.data())},
        {QStringLiteral("dialogTitle"),
         title.isEmpty() ? (save ? QStringLiteral("Save File") : QStringLiteral("Open File"))
                         : title},
        {QStringLiteral("saveMode"), save},
        {QStringLiteral("modalDialog"), options.value(QStringLiteral("modal"), true).toBool()},
        {QStringLiteral("multiple"), pending->multiple},
        {QStringLiteral("directoryMode"), pending->directory},
        {QStringLiteral("nameFilters"),
         decodeNameFilters(options.value(QStringLiteral("filters")))},
        {QStringLiteral("acceptLabel"),
         options.value(QStringLiteral("accept_label")).toString()},
        {QStringLiteral("suggestedName"),
         options.value(QStringLiteral("current_name")).toString()}};

    auto folder = pathOption(options, QStringLiteral("current_folder"));
    if (save) {
        const auto currentFile = pathOption(options, QStringLiteral("current_file"));
        if (!currentFile.isEmpty()) {
            const QFileInfo info(currentFile);
            folder = info.absolutePath();
            properties.insert(QStringLiteral("suggestedName"), info.fileName());
        }
    }
    if (folder.isEmpty() || !QFileInfo(folder).isDir()) {
        folder = QDir::homePath();
    }
    properties.insert(QStringLiteral("initialFolder"),
                      QUrl::fromLocalFile(folder).toString());

    m_pending.insert(path, pending);

    connect(pending->request.data(), &PortalRequest::accepted, this,
            [this, path](const QVariant &value) {
                QTimer::singleShot(0, this, [this, path, value] {
                    const auto pending = m_pending.value(path);
                    if (!pending) {
                        return;
                    }
                    auto uris = selectedUris(value, pending->directory);
                    if (!pending->multiple && uris.size() > 1) {
                        uris = {uris.constFirst()};
                    }
                    if (pending->save && uris.size() != 1) {
                        finish(path, Failed, {});
                        return;
                    }
                    if (uris.isEmpty()) {
                        finish(path, Cancelled, {});
                        return;
                    }
                    finish(path, Success,
                           {{QStringLiteral("uris"), uris},
                            {QStringLiteral("writable"), pending->save}});
                });
            });
    connect(pending->request.data(), &PortalRequest::cancelled, this,
            [this, path] {
                QTimer::singleShot(0, this, [this, path] {
                    finish(path, Cancelled, {});
                });
            });

    QQmlComponent component(&m_engine, QUrl(QStringLiteral("qrc:/nx/qml/src/qml/FileChooserDialog.qml")));
    if (component.status() != QQmlComponent::Ready) {
        m_pending.remove(path);
        m_connection.send(message.createReply(
            {Failed, QVariantMap{{QStringLiteral("error"), component.errorString()}}}));
        return;
    }

    auto created = std::unique_ptr<QObject>(component.createWithInitialProperties(properties));
    if (!created) {
        m_pending.remove(path);
        m_connection.send(message.createReply(
            {Failed, QVariantMap{{QStringLiteral("error"), component.errorString()}}}));
        return;
    }
    created->setParent(pending->request.data());
    pending->dialog = created.get();
    created.release();
}

void FileChooser::finish(const QString &path, uint response, const QVariantMap &results)
{
    const auto pending = m_pending.take(path);
    if (!pending) {
        return;
    }
    m_connection.send(pending->call.createReply({response, results}));
}

