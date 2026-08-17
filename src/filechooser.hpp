// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QHash>
#include <QPointer>
#include <QSharedPointer>
#include <QStringList>
#include <QVariantMap>

class QQmlEngine;
class PortalRequest;

class FileChooser final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.FileChooser")
    Q_CLASSINFO("D-Bus Introspection",
        "  <interface name=\"org.freedesktop.impl.portal.FileChooser\">\n"
        "    <method name=\"OpenFile\">\n"
        "      <arg direction=\"in\" type=\"o\" name=\"handle\"/>\n"
        "      <arg direction=\"in\" type=\"s\" name=\"app_id\"/>\n"
        "      <arg direction=\"in\" type=\"s\" name=\"parent_window\"/>\n"
        "      <arg direction=\"in\" type=\"s\" name=\"title\"/>\n"
        "      <arg direction=\"in\" type=\"a{sv}\" name=\"options\"/>\n"
        "      <arg direction=\"out\" type=\"u\" name=\"response\"/>\n"
        "      <arg direction=\"out\" type=\"a{sv}\" name=\"results\"/>\n"
        "    </method>\n"
        "    <method name=\"SaveFile\">\n"
        "      <arg direction=\"in\" type=\"o\" name=\"handle\"/>\n"
        "      <arg direction=\"in\" type=\"s\" name=\"app_id\"/>\n"
        "      <arg direction=\"in\" type=\"s\" name=\"parent_window\"/>\n"
        "      <arg direction=\"in\" type=\"s\" name=\"title\"/>\n"
        "      <arg direction=\"in\" type=\"a{sv}\" name=\"options\"/>\n"
        "      <arg direction=\"out\" type=\"u\" name=\"response\"/>\n"
        "      <arg direction=\"out\" type=\"a{sv}\" name=\"results\"/>\n"
        "    </method>\n"
        "  </interface>\n")

public:
    FileChooser(QObject &parent, QQmlEngine &engine, QDBusConnection connection);

public slots:
    void OpenFile(const QDBusObjectPath &handle,
                  const QString &appId,
                  const QString &parentWindow,
                  const QString &title,
                  const QVariantMap &options,
                  const QDBusMessage &message);
    void SaveFile(const QDBusObjectPath &handle,
                  const QString &appId,
                  const QString &parentWindow,
                  const QString &title,
                  const QVariantMap &options,
                  const QDBusMessage &message);

private:
    struct Pending;
    void begin(const QDBusObjectPath &handle,
               const QString &title,
               const QVariantMap &options,
               const QDBusMessage &message,
               bool save);
    void showNextDialog();
    void showDialog(const QString &path,
                    const QSharedPointer<Pending> &pending,
                    const QVariantMap &properties,
                    int attempt = 0);
    void finish(const QString &path, uint response, const QVariantMap &results);

    QQmlEngine &m_engine;
    QDBusConnection m_connection;
    QHash<QString, QSharedPointer<Pending>> m_pending;
    QPointer<QObject> m_dialog;
    QStringList m_dialogQueue;
    QString m_activeDialogPath;
};

