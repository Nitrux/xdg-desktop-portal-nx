// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 Nitrux Latinoamericana S.C. <hello@nxos.org>

#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QHash>
#include <QSharedPointer>
#include <QVariantMap>

class InhibitAdaptor final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Inhibit")
    Q_CLASSINFO("D-Bus Introspection",
        "  <interface name=\"org.freedesktop.impl.portal.Inhibit\">\n"
        "    <method name=\"Inhibit\">\n"
        "      <arg direction=\"in\" type=\"o\" name=\"handle\"/>\n"
        "      <arg direction=\"in\" type=\"s\" name=\"app_id\"/>\n"
        "      <arg direction=\"in\" type=\"s\" name=\"window\"/>\n"
        "      <arg direction=\"in\" type=\"u\" name=\"flags\"/>\n"
        "      <arg direction=\"in\" type=\"a{sv}\" name=\"options\"/>\n"
        "    </method>\n"
        "  </interface>\n")

public:
    InhibitAdaptor(QObject &parent, QDBusConnection connection);

public slots:
    void Inhibit(const QDBusObjectPath &handle,
                 const QString &appId,
                 const QString &window,
                 uint flags,
                 const QVariantMap &options,
                 const QDBusMessage &message);

private:
    struct Pending;
    void finish(const QString &path);

    QDBusConnection m_connection;
    QHash<QString, QSharedPointer<Pending>> m_pending;
};
