// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 Nitrux Latinoamericana S.C. <hello@nxos.org>

#include "inhibit.hpp"

#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusUnixFileDescriptor>
#include <QDebug>
#include <QMetaType>
#include <QTimer>
#include <utility>

namespace
{
constexpr uint Logout = 1U;
constexpr uint UserSwitch = 2U;
constexpr uint Suspend = 4U;
constexpr uint Idle = 8U;
constexpr uint SupportedFlags = Suspend | Idle;

constexpr auto LoginService = "org.freedesktop.login1";
constexpr auto LoginPath = "/org/freedesktop/login1";
constexpr auto LoginInterface = "org.freedesktop.login1.Manager";

class InhibitRequest;

class RequestAdaptor final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Request")

public:
    explicit RequestAdaptor(InhibitRequest &request);

public slots:
    void Close(const QDBusMessage &message);

private:
    InhibitRequest &m_request;
};

class InhibitRequest final : public QObject
{
    Q_OBJECT

public:
    InhibitRequest(QDBusConnection connection, QString objectPath)
        : m_connection(std::move(connection))
        , m_objectPath(std::move(objectPath))
        , m_adaptor(std::make_unique<RequestAdaptor>(*this))
    {
    }

    ~InhibitRequest() override
    {
        if (m_registered)
            m_connection.unregisterObject(m_objectPath);
    }

    [[nodiscard]] bool inhibitIdle(const QString &appId,
                                   const QString &reason,
                                   QString &error)
    {
        return inhibitLogin(QStringLiteral("idle"), appId, reason, m_idleFd, error);
    }

    [[nodiscard]] bool inhibitSuspend(const QString &appId,
                                      const QString &reason,
                                      QString &error)
    {
        return inhibitLogin(QStringLiteral("sleep"), appId, reason, m_suspendFd, error);
    }

    [[nodiscard]] bool registerObject()
    {
        m_registered = m_connection.registerObject(
            m_objectPath, this, QDBusConnection::ExportAdaptors);
        return m_registered;
    }

signals:
    void closeRequested();

private:
    [[nodiscard]] static bool inhibitLogin(const QString &what,
                                           const QString &appId,
                                           const QString &reason,
                                           QDBusUnixFileDescriptor &fd,
                                           QString &error)
    {
        QDBusInterface login(QString::fromLatin1(LoginService),
                             QString::fromLatin1(LoginPath),
                             QString::fromLatin1(LoginInterface),
                             QDBusConnection::systemBus());
        if (!login.isValid()) {
            error = QStringLiteral("The elogind session inhibitor is unavailable");
            return false;
        }

        const auto who = appId.isEmpty()
            ? QStringLiteral("xdg-desktop-portal-nx")
            : appId;
        const QDBusReply<QDBusUnixFileDescriptor> reply = login.call(
            QStringLiteral("Inhibit"), what, who, reason,
            QStringLiteral("block"));
        if (!reply.isValid() || !reply.value().isValid()) {
            error = reply.isValid()
                ? QStringLiteral("elogind returned an invalid session inhibitor")
                : reply.error().message();
            return false;
        }

        fd = reply.value();
        return true;
    }
    friend class RequestAdaptor;

    QDBusConnection m_connection;
    QString m_objectPath;
    bool m_registered{false};
    QDBusUnixFileDescriptor m_idleFd;
    QDBusUnixFileDescriptor m_suspendFd;
    std::unique_ptr<RequestAdaptor> m_adaptor;
};

RequestAdaptor::RequestAdaptor(InhibitRequest &request)
    : QDBusAbstractAdaptor(&request)
    , m_request(request)
{
}

void RequestAdaptor::Close(const QDBusMessage &message)
{
    Q_UNUSED(message)
    emit m_request.closeRequested();
}
}

struct InhibitAdaptor::Pending
{
    QSharedPointer<InhibitRequest> request;
    QDBusServiceWatcher *callerWatcher{nullptr};
};

InhibitAdaptor::InhibitAdaptor(QObject &parent, QDBusConnection connection)
    : QDBusAbstractAdaptor(&parent)
    , m_connection(std::move(connection))
{
}

void InhibitAdaptor::Inhibit(const QDBusObjectPath &handle,
                      const QString &appId,
                      const QString &window,
                      uint flags,
                      const QVariantMap &options,
                      const QDBusMessage &message)
{
    Q_UNUSED(window)
    message.setDelayedReply(true);

    const auto path = handle.path();
    const auto fail = [this, &message](const QString &name, const QString &reason) {
        m_connection.send(message.createErrorReply(name, reason));
    };

    if (path.isEmpty() || m_pending.contains(path)) {
        fail(QStringLiteral("org.freedesktop.portal.Error.InvalidArgument"),
             QStringLiteral("Invalid or duplicate request handle"));
        return;
    }

    if (flags == 0U || (flags & ~SupportedFlags) != 0U) {
        const auto unsupported = flags & (Logout | UserSwitch);
        const auto reason = unsupported != 0U
            ? QStringLiteral("Logout and user-switch inhibition are not supported by this session")
            : QStringLiteral("The requested inhibition flags are not supported");
        fail(QStringLiteral("org.freedesktop.portal.Error.NotAllowed"), reason);
        return;
    }

    const auto reasonValue = options.value(QStringLiteral("reason"));
    if (reasonValue.isValid() && reasonValue.metaType().id() != QMetaType::fromType<QString>().id()) {
        fail(QStringLiteral("org.freedesktop.portal.Error.InvalidArgument"),
             QStringLiteral("The reason option must be a string"));
        return;
    }
    auto reason = reasonValue.toString();
    if (reason.isEmpty())
        reason = appId;

    auto pending = QSharedPointer<Pending>::create();
    pending->request = QSharedPointer<InhibitRequest>::create(m_connection, path);
    QString error;
    if ((flags & Idle) != 0U && !pending->request->inhibitIdle(appId, reason, error)) {
        fail(QStringLiteral("org.freedesktop.portal.Error.Failed"), error);
        return;
    }
    if ((flags & Suspend) != 0U && !pending->request->inhibitSuspend(appId, reason, error)) {
        fail(QStringLiteral("org.freedesktop.portal.Error.Failed"), error);
        return;
    }

    if (!pending->request->registerObject()) {
        fail(QStringLiteral("org.freedesktop.portal.Error.Failed"),
             QStringLiteral("Could not export the request object"));
        return;
    }

    const auto sender = message.service();
    auto *interface = m_connection.interface();
    const auto ownerReply = interface ? interface->serviceOwner(sender) : QDBusReply<QString>();
    if (!ownerReply.isValid() || ownerReply.value().isEmpty()) {
        fail(QStringLiteral("org.freedesktop.portal.Error.Failed"),
             QStringLiteral("Could not track the request caller"));
        return;
    }

    const auto owner = ownerReply.value();
    pending->callerWatcher = new QDBusServiceWatcher(
        owner, m_connection, QDBusServiceWatcher::WatchForUnregistration, this);
    connect(pending->callerWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, [this, path](const QString &) {
                finish(path);
            });
    connect(pending->request.data(), &InhibitRequest::closeRequested,
            this, [this, path] {
                QTimer::singleShot(0, this, [this, path] {
                    finish(path);
                });
            });

    m_pending.insert(path, pending);
    qInfo() << "Inhibit request started" << path << "appId" << appId
            << "flags" << flags << "reason" << reason;
    m_connection.send(message.createReply());
}

void InhibitAdaptor::finish(const QString &path)
{
    const auto pending = m_pending.take(path);
    if (!pending)
        return;

    qInfo() << "Inhibit request closed" << path;
    pending->callerWatcher->deleteLater();
}

#include "inhibit.moc"
