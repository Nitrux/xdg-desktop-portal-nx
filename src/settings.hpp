// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusArgument>
#include <QDBusContext>
#include <QDBusVariant>
#include <QFileSystemWatcher>
#include <QMap>
#include <QTimer>
#include <QVariantMap>

using PortalSettings = QMap<QString, QVariantMap>;
Q_DECLARE_METATYPE(PortalSettings)

QDBusArgument &operator<<(QDBusArgument &argument, const PortalSettings &settings);
const QDBusArgument &operator>>(const QDBusArgument &argument, PortalSettings &settings);

class QEvent;

class Settings final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Settings")
    Q_PROPERTY(uint version READ version CONSTANT)

public:
    Settings(QObject &parent, QDBusContext &context);
    [[nodiscard]] static constexpr uint version() noexcept { return 1U; }

public slots:
    [[nodiscard]] QDBusVariant Read(const QString &nameSpace, const QString &key);
    [[nodiscard]] PortalSettings ReadAll(const QStringList &namespaces) const;

signals:
    void SettingChanged(const QString &nameSpace, const QString &key, const QDBusVariant &value);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void scheduleReload(const QString &path);
    void reload();

private:
    [[nodiscard]] static QString gsettings(const QString &schema, const QString &key);
    [[nodiscard]] static bool namespaceMatches(const QString &nameSpace,
                                               const QStringList &patterns);
    [[nodiscard]] PortalSettings readHostSettings() const;
    void refreshWatches();

    PortalSettings m_values;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;
    QStringList m_configFiles;
    QStringList m_configDirectories;
    QStringList m_fontconfigFiles;
    QStringList m_fontconfigDirectories;
    int m_fontconfigSerial = 0;
    bool m_fontconfigChanged = false;
    QDBusContext &m_context;
};
