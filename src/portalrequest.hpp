// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QVariant>
#include <memory>

class RequestAdaptor;

class PortalRequest final : public QObject
{
    Q_OBJECT

public:
    PortalRequest(QDBusConnection connection, QString objectPath);
    ~PortalRequest() override;

    PortalRequest(const PortalRequest &) = delete;
    PortalRequest &operator=(const PortalRequest &) = delete;

    [[nodiscard]] bool registerObject();
    [[nodiscard]] const QString &objectPath() const noexcept;

    Q_INVOKABLE void accept(const QVariant &value);
    Q_INVOKABLE void reject();

signals:
    void accepted(const QVariant &value);
    void cancelled();
    void closeUiRequested();

private:
    friend class RequestAdaptor;
    void close();

    QDBusConnection m_connection;
    QString m_objectPath;
    bool m_registered{false};
    bool m_completed{false};
    std::unique_ptr<RequestAdaptor> m_adaptor;
};

