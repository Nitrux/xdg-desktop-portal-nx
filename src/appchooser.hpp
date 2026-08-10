// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QHash>
#include <QSharedPointer>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class QQmlEngine;

class AppChooser final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.AppChooser")
    Q_CLASSINFO("D-Bus Introspection",
        "  <interface name=\"org.freedesktop.impl.portal.AppChooser\">\n"
        "    <property name=\"version\" type=\"u\" access=\"read\"/>\n"
        "    <method name=\"ChooseApplication\">\n"
        "      <arg direction=\"in\" type=\"o\" name=\"handle\"/>\n"
        "      <arg direction=\"in\" type=\"s\" name=\"app_id\"/>\n"
        "      <arg direction=\"in\" type=\"s\" name=\"parent_window\"/>\n"
        "      <arg direction=\"in\" type=\"as\" name=\"choices\"/>\n"
        "      <arg direction=\"in\" type=\"a{sv}\" name=\"options\"/>\n"
        "      <arg direction=\"out\" type=\"u\" name=\"response\"/>\n"
        "      <arg direction=\"out\" type=\"a{sv}\" name=\"results\"/>\n"
        "    </method>\n"
        "    <method name=\"UpdateChoices\">\n"
        "      <arg direction=\"in\" type=\"o\" name=\"handle\"/>\n"
        "      <arg direction=\"in\" type=\"as\" name=\"choices\"/>\n"
        "    </method>\n"
        "  </interface>\n")
    Q_PROPERTY(uint version READ version CONSTANT)

public:
    AppChooser(QObject &parent, QQmlEngine &engine, QDBusConnection connection);
    [[nodiscard]] static constexpr uint version() noexcept { return 2U; }

public slots:
    void ChooseApplication(const QDBusObjectPath &handle,
                           const QString &appId,
                           const QString &parentWindow,
                           const QStringList &choices,
                           const QVariantMap &options,
                           const QDBusMessage &message);
    void UpdateChoices(const QDBusObjectPath &handle, const QStringList &choices);

private:
    struct Pending;
    [[nodiscard]] static QVariantList applications(const QStringList &choices);
    void finish(const QString &path, uint response, const QVariantMap &results);

    QQmlEngine &m_engine;
    QDBusConnection m_connection;
    QHash<QString, QSharedPointer<Pending>> m_pending;
};

