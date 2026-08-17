// SPDX-License-Identifier: BSD-3-Clause
#include "openuri.hpp"
#include <QProcess>
#include <QUrl>
#include <utility>
OpenUri::OpenUri(QObject &parent, QDBusConnection connection)
    : QDBusAbstractAdaptor(&parent), m_connection(std::move(connection)) { setAutoRelaySignals(true); }
void OpenUri::OpenURI(const QDBusObjectPath &handle, const QString &appId,
                      const QString &parentWindow, const QString &uri,
                      const QVariantMap &options, const QDBusMessage &message)
{
    Q_UNUSED(handle)
    Q_UNUSED(appId)
    Q_UNUSED(parentWindow)
    Q_UNUSED(options)
    message.setDelayedReply(true);
    const QUrl url(uri);
    if (!url.isValid() || url.scheme().isEmpty() || url.scheme() == QStringLiteral("file")) {
        m_connection.send(message.createReply({2U, QVariantMap{{QStringLiteral("error"), QStringLiteral("Unsupported URI")}}}));
        return;
    }
    qInfo() << "OpenURI launching" << uri << "with xdg-open";
    if (!QProcess::startDetached(QStringLiteral("xdg-open"), {uri})) {
        m_connection.send(message.createReply({2U, QVariantMap{{QStringLiteral("error"), QStringLiteral("Could not start xdg-open")}}}));
        return;
    }
    m_connection.send(message.createReply({0U, QVariantMap{}}));
}

#include <QFileInfo>

void OpenUri::OpenFile(const QDBusObjectPath &handle, const QString &appId,
                       const QString &parentWindow, const QDBusUnixFileDescriptor &fd,
                       const QVariantMap &options, const QDBusMessage &message)
{
    Q_UNUSED(handle)
    Q_UNUSED(appId)
    Q_UNUSED(parentWindow)
    Q_UNUSED(options)
    message.setDelayedReply(true);
    const auto path = QFileInfo(QStringLiteral("/proc/self/fd/%1").arg(fd.fileDescriptor())).symLinkTarget();
    if (path.isEmpty() || !QProcess::startDetached(QStringLiteral("xdg-open"), {path})) {
        m_connection.send(message.createReply({2U, QVariantMap{{QStringLiteral("error"), QStringLiteral("Could not open file")}}}));
        return;
    }
    qInfo() << "OpenFile launching" << path << "with xdg-open";
    m_connection.send(message.createReply({0U, QVariantMap{}}));
}
