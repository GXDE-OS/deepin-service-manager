// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "serviceqtdbus.h"

#include "policy/policy.h"
#include "qtdbushook.h"

#include <QDBusAbstractAdaptor>
#include <QDebug>
#include <QFileInfo>
#include <QLibrary>
#include <QMetaClassInfo>
#include <QThread>

ServiceQtDBus::ServiceQtDBus(QObject *parent)
    : ServiceBase(parent)
    , m_library(nullptr)
{
    m_SDKType = SDKType::QT;
}

QDBusConnection ServiceQtDBus::qDbusConnection()
{
    if (policy->name.isEmpty()) {
        if (m_sessionType == QDBusConnection::SystemBus) {
            return QDBusConnection::systemBus();
        } else {
            return QDBusConnection::sessionBus();
        }
    } else {
        if (m_sessionType == QDBusConnection::SystemBus) {
            return QDBusConnection::connectToBus(QDBusConnection::SystemBus, policy->name);
        } else {
            return QDBusConnection::connectToBus(QDBusConnection::SessionBus, policy->name);
        }
    }
}

void ServiceQtDBus::initThread()
{
    qInfo() << "[ServiceQtDBus]init service: " << policy->paths();
    qDbusConnection().registerService(policy->name);
    qDbusConnection().registerObject(QStringLiteral("/PrivateDeclaration"), this);

    // TODO:无权限、隐藏、按需启动需求的service，不应该注册，避免触发hook，提高效率
    QTDbusHook::instance()->setServiceObject(this);

    QFileInfo fileInfo(QString(SERVICE_LIB_DIR) + policy->pluginPath);
    if (QLibrary::isLibrary(fileInfo.absoluteFilePath())) {
        qInfo() << "[ServiceQtDBus]init library: " << fileInfo.absoluteFilePath();
        m_library = new QLibrary(fileInfo.absoluteFilePath());
    }

    registerService();
    ServiceBase::initThread();
}

bool ServiceQtDBus::registerService()
{
    qInfo() << "[ServiceQtDBus]service register: " << policy->name;

    if (libFuncCall("DSMRegister", true)) {
        ServiceBase::registerService();
        return true;
    } else {
        return false;
    }
}

bool ServiceQtDBus::unregisterService()
{
    qInfo() << "[ServiceQtDBus]service unregister: " << policy->name;

    if (libFuncCall("DSMUnRegister", false)) {
        ServiceBase::unregisterService();
        return true;
    } else {
        return false;
    }
}

bool ServiceQtDBus::libFuncCall(const QString &funcName, bool isRegister)
{
    if (m_library == nullptr) {
        return false;
    }
    auto objFunc = isRegister ? DSMRegister(m_library->resolve(funcName.toStdString().c_str()))
                              : DSMUnRegister(m_library->resolve(funcName.toStdString().c_str()));
    if (!objFunc) {
        qWarning() << QString("[ServiceSDBus]failed to resolve the `%1` method: ").arg(funcName)
                   << m_library->fileName();
        if (m_library->isLoaded())
            m_library->unload();
        m_library->deleteLater();
        return false;
    }
    auto connection = qDbusConnection();
    int ret = objFunc(policy->name.toStdString().c_str(), (void *)&connection);
    if (ret) {
        return false;
    }
    return true;
}
