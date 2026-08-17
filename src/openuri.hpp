// SPDX-License-Identifier: BSD-3-Clause
#pragma once
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <QVariantMap>
class OpenUri final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.OpenURI")
    Q_PROPERTY(uint version READ version CONSTANT)
public:
    OpenUri(QObject &parent, QDBusConnection connection);
    [[nodiscard]] static constexpr uint version() noexcept { return 1U; }
public slots:
    void OpenURI(const QDBusObjectPath &handle, const QString &appId, const QString &parentWindow,
                 const QString &uri, const QVariantMap &options, const QDBusMessage &message);
    void OpenFile(const QDBusObjectPath &handle, const QString &appId,
                  const QString &parentWindow, const QDBusUnixFileDescriptor &fd,
                  const QVariantMap &options, const QDBusMessage &message);
private:
    QDBusConnection m_connection;
};
