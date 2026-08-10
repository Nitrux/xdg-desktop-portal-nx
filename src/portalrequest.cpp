// SPDX-License-Identifier: BSD-3-Clause
#include "portalrequest.hpp"

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <utility>

class RequestAdaptor final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Request")

public:
    explicit RequestAdaptor(PortalRequest &request)
        : QDBusAbstractAdaptor(&request)
        , m_request(request)
    {
    }

public slots:
    void Close()
    {
        m_request.close();
    }

private:
    PortalRequest &m_request;
};

PortalRequest::PortalRequest(QDBusConnection connection, QString objectPath)
    : m_connection(std::move(connection))
    , m_objectPath(std::move(objectPath))
{
    m_adaptor = std::make_unique<RequestAdaptor>(*this);
}

PortalRequest::~PortalRequest()
{
    if (m_registered) {
        m_connection.unregisterObject(m_objectPath);
    }
}

bool PortalRequest::registerObject()
{
    if (m_registered || m_objectPath.isEmpty()) {
        return false;
    }

    m_registered = m_connection.registerObject(
        m_objectPath,
        this,
        QDBusConnection::ExportAdaptors);
    return m_registered;
}

const QString &PortalRequest::objectPath() const noexcept
{
    return m_objectPath;
}

void PortalRequest::accept(const QVariant &value)
{
    if (m_completed) {
        return;
    }

    m_completed = true;
    emit accepted(value);
}

void PortalRequest::reject()
{
    if (m_completed) {
        return;
    }

    m_completed = true;
    emit cancelled();
}

void PortalRequest::close()
{
    if (m_completed) {
        return;
    }

    m_completed = true;
    emit closeUiRequested();
    emit cancelled();
}

#include "portalrequest.moc"

