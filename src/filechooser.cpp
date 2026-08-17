// SPDX-License-Identifier: BSD-3-Clause
#include "filechooser.hpp"
#include "portalrequest.hpp"

#include <QByteArray>
#include <QDBusArgument>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QMetaObject>
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

QString responseName(uint response)
{
    switch (response) {
    case Success:
        return QStringLiteral("success");
    case Cancelled:
        return QStringLiteral("cancelled");
    case Failed:
        return QStringLiteral("failed");
    default:
        return QStringLiteral("unknown");
    }
}

QVariantMap failureResults(const QString &stage, const QString &error)
{
    return {{QStringLiteral("error"), error},
            {QStringLiteral("nx_stage"), stage}};
}

QString pathOption(const QVariantMap &options, const QString &name)
{
    auto bytes = options.value(name).toByteArray();
    while (bytes.endsWith('\0')) {
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

QStringList selectedUris(const QVariant &value, bool directory, QStringList *rejections)
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
            if (rejections)
                rejections->append(QStringLiteral("Not a local file URI: %1").arg(url.toString()));
            continue;
        }

        const QFileInfo info(url.toLocalFile());
        if (directory != info.isDir()) {
            if (rejections) {
                rejections->append(directory
                                       ? QStringLiteral("Not a directory: %1").arg(info.absoluteFilePath())
                                       : QStringLiteral("Directory returned while selecting files: %1").arg(info.absoluteFilePath()));
            }
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
    QDBusMessage call;
    QVariantMap properties;
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

    QQmlComponent component(&m_engine,
                            QUrl(QStringLiteral("qrc:/nx/qml/src/qml/FileChooserDialog.qml")));
    if (component.status() != QQmlComponent::Ready)
        qWarning() << "Could not preload file chooser QML:" << component.errorString();
}

void FileChooser::OpenFile(const QDBusObjectPath &handle,
                           const QString &appId,
                           const QString &parentWindow,
                           const QString &title,
                           const QVariantMap &options,
                           const QDBusMessage &message)
{
    qInfo().noquote() << QStringLiteral("FileChooser request path=%1 method=OpenFile sender=%2 appId=%3 parent=%4 title=%5")
                             .arg(handle.path(), message.service(), appId, parentWindow, title)
                      << "options=" << options;
    begin(handle, title, options, message, false);
}

void FileChooser::SaveFile(const QDBusObjectPath &handle,
                           const QString &appId,
                           const QString &parentWindow,
                           const QString &title,
                           const QVariantMap &options,
                           const QDBusMessage &message)
{
    qInfo().noquote() << QStringLiteral("FileChooser request path=%1 method=SaveFile sender=%2 appId=%3 parent=%4 title=%5")
                             .arg(handle.path(), message.service(), appId, parentWindow, title)
                      << "options=" << options;
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
        qWarning().noquote() << QStringLiteral("FileChooser rejected request path=%1: %2")
                                    .arg(path, path.isEmpty()
                                                   ? QStringLiteral("empty request handle")
                                                   : QStringLiteral("request handle is still active"));
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
        const auto error = m_connection.lastError();
        qWarning().noquote() << QStringLiteral("FileChooser could not export request path=%1: %2: %3")
                                    .arg(path, error.name(), error.message());
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
    if (!folder.isEmpty() && !QDir::isAbsolutePath(folder)) {
        qWarning() << "FileChooser ignored relative current_folder" << folder
                   << "for" << path;
        folder.clear();
    }
    if (save) {
        const auto currentFile = pathOption(options, QStringLiteral("current_file"));
        if (!currentFile.isEmpty() && QDir::isAbsolutePath(currentFile)) {
            const QFileInfo info(currentFile);
            folder = info.absolutePath();
            properties.insert(QStringLiteral("suggestedName"), info.fileName());
        } else if (!currentFile.isEmpty()) {
            qWarning() << "FileChooser ignored relative current_file" << currentFile
                       << "for" << path;
        }
    }
    if (folder.isEmpty() || !QFileInfo(folder).isDir()) {
        folder = QDir::homePath();
    }
    properties.insert(QStringLiteral("initialFolder"),
                      QUrl::fromLocalFile(folder).toString());

    qInfo().noquote() << QStringLiteral("FileChooser prepared path=%1 mode=%2 directory=%3 multiple=%4 folder=%5 filters=%6")
                             .arg(path, save ? QStringLiteral("save") : QStringLiteral("open"))
                             .arg(pending->directory)
                             .arg(pending->multiple)
                             .arg(folder, properties.value(QStringLiteral("nameFilters")).toStringList().join(QStringLiteral(", ")));

    pending->properties = properties;
    m_pending.insert(path, pending);

    connect(pending->request.data(), &PortalRequest::accepted, this,
            [this, path](const QVariant &value) {
                QTimer::singleShot(0, this, [this, path, value] {
                    const auto pending = m_pending.value(path);
                    if (!pending) {
                        qWarning() << "FileChooser ignored a late accepted signal for" << path;
                        return;
                    }
                    qInfo().noquote() << QStringLiteral("FileChooser selection path=%1 type=%2 raw=")
                                             .arg(path, QString::fromLatin1(value.metaType().name()))
                                      << value;
                    QStringList rejectedSelections;
                    auto uris = selectedUris(value, pending->directory, &rejectedSelections);
                    if (!pending->multiple && uris.size() > 1) {
                        uris = {uris.constFirst()};
                    }
                    qInfo() << "FileChooser validated selection" << path << uris;
                    if (!rejectedSelections.isEmpty())
                        qWarning() << "FileChooser rejected selection entries" << path << rejectedSelections;
                    if (pending->save && uris.size() != 1) {
                        finish(path, Failed,
                               failureResults(QStringLiteral("selection-validation"),
                                              QStringLiteral("SaveFile requires exactly one local URI; received %1")
                                                  .arg(uris.size())));
                        return;
                    }
                    if (uris.isEmpty()) {
                        qInfo() << "FileChooser completed without a valid local selection; treating as cancellation:" << path;
                        finish(path, Cancelled, {});
                        return;
                    }
                    finish(path, Success,
                           {{QStringLiteral("uris"), uris},
                            {QStringLiteral("writable"), true}});
                });
            });
    connect(pending->request.data(), &PortalRequest::cancelled, this,
            [this, path] {
                qInfo() << "FileChooser UI rejected or closed:" << path;
                QTimer::singleShot(0, this, [this, path] {
                    finish(path, Cancelled, {});
                });
            });

    m_dialogQueue.append(path);
    qInfo() << "FileChooser queued" << path
            << "position" << m_dialogQueue.size()
            << "active" << m_activeDialogPath;
    showNextDialog();

}

void FileChooser::showNextDialog()
{
    if (!m_activeDialogPath.isEmpty()) {
        qInfo() << "FileChooser waiting; active dialog" << m_activeDialogPath
                << "queued" << m_dialogQueue.size();
        return;
    }

    while (!m_dialogQueue.isEmpty()) {
        const auto path = m_dialogQueue.takeFirst();
        const auto pending = m_pending.value(path);
        if (!pending)
            continue;

        m_activeDialogPath = path;
        qInfo() << "FileChooser activating" << path
                << "remaining queued" << m_dialogQueue.size();
        QTimer::singleShot(0, this, [this, path, pending] {
            if (m_activeDialogPath != path || !m_pending.contains(path))
                return;
            showDialog(path, pending, pending->properties);
        });
        return;
    }
}

void FileChooser::showDialog(const QString &path,
                             const QSharedPointer<Pending> &pending,
                             const QVariantMap &properties,
                             int attempt)
{
    if (!m_pending.contains(path) || m_activeDialogPath != path) {
        qWarning() << "FileChooser skipped presentation for inactive request" << path
                   << "attempt" << attempt;
        return;
    }

    const auto present = [this, &properties, &path] {
        for (auto it = properties.cbegin(); it != properties.cend(); ++it) {
            if (!m_dialog->setProperty(it.key().toUtf8().constData(), it.value()))
                qWarning() << "FileChooser could not set persistent dialog property"
                           << it.key() << "for" << path;
        }
        qInfo() << "FileChooser presenting persistent QML window" << path << m_dialog.data();
        if (!QMetaObject::invokeMethod(m_dialog, "present")) {
            finish(path, Failed,
                   failureResults(QStringLiteral("qml-presentation"),
                                  QStringLiteral("Could not invoke FileChooserDialog.present")));
        }
    };

    if (m_dialog) {
        present();
        return;
    }

    qInfo() << "FileChooser creating persistent QML window" << path << "attempt" << attempt;
    QQmlComponent component(&m_engine,
                            QUrl(QStringLiteral("qrc:/nx/qml/src/qml/FileChooserDialog.qml")));
    if (component.status() != QQmlComponent::Ready) {
        qWarning() << "File chooser QML is not ready (attempt" << attempt << "):"
                   << component.errorString();
        if (attempt < 2) {
            QTimer::singleShot(100, this, [this, path, pending, properties, attempt] {
                showDialog(path, pending, properties, attempt + 1);
            });
            return;
        }
        finish(path, Failed,
               failureResults(QStringLiteral("qml-component"), component.errorString()));
        return;
    }

    auto created = std::unique_ptr<QObject>(component.createWithInitialProperties(properties));
    if (!created) {
        qWarning() << "Could not create file chooser QML (attempt" << attempt << "):"
                   << component.errorString();
        if (attempt < 2) {
            QTimer::singleShot(100, this, [this, path, pending, properties, attempt] {
                showDialog(path, pending, properties, attempt + 1);
            });
            return;
        }
        finish(path, Failed,
               failureResults(QStringLiteral("qml-object-creation"), component.errorString()));
        return;
    }

    QQmlEngine::setObjectOwnership(created.get(), QQmlEngine::CppOwnership);
    created->setParent(this);
    m_dialog = created.release();
    qInfo() << "FileChooser persistent QML window created" << m_dialog.data();
    present();
}

void FileChooser::finish(const QString &path, uint response, const QVariantMap &results)
{
    const auto pending = m_pending.value(path);
    if (!pending) {
        return;
    }

    const bool wasActive = m_activeDialogPath == path;
    m_dialogQueue.removeAll(path);
    if (wasActive)
        m_activeDialogPath.clear();

    qInfo().noquote() << QStringLiteral("FileChooser completing path=%1 response=%2 (%3)")
                             .arg(path)
                             .arg(response)
                             .arg(responseName(response))
                      << "results=" << results;

    if (wasActive && m_dialog) {
        if (!QMetaObject::invokeMethod(m_dialog, "dismiss")) {
            qWarning() << "FileChooser could not dismiss persistent QML window for"
                       << path;
        }
    }

    m_pending.remove(path);
    if (!m_connection.send(pending->call.createReply({response, results}))) {
        const auto error = m_connection.lastError();
        qWarning().noquote() << QStringLiteral("FileChooser could not send reply path=%1: %2: %3")
                                    .arg(path, error.name(), error.message());
    }

    if (wasActive)
        QTimer::singleShot(0, this, [this] { showNextDialog(); });
}

