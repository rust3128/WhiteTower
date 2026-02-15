#include "StationDataContext.h"
#include "Oracle/ApiClient.h"
#include "Oracle/Logger.h"

StationDataContext::StationDataContext(int objectId, QObject *parent)
    : QObject(parent), m_objectId(objectId)
{
}

void StationDataContext::fetchGeneralInfo()
{
    logInfo() << "DataContext: Fetching general info for Object ID:" << m_objectId;

    // 1. ВИПРАВЛЕНО: Правильний сигнал від ApiClient
    // 2. ВИПРАВЛЕНО: Прибрано SingleShotConnection для надійності
    connect(&ApiClient::instance(), &ApiClient::objectGeneralInfoFetched,
            this, &StationDataContext::onApiDataReceived);

    ApiClient::instance().fetchObjectGeneralInfo(m_objectId);
}

void StationDataContext::onApiDataReceived(int fetchedObjectId, const QJsonObject &data)
{
    if (fetchedObjectId != m_objectId) {
        return;
    }

    logInfo() << "DataContext: Received general info for Object ID:" << m_objectId;

    m_generalInfo.clientId   = data["CLIENT_ID"].toInt();
    m_generalInfo.terminalId = data["TERMINAL_ID"].toInt();
    m_generalInfo.clientName = data["CLIENT_NAME"].toString();
    m_generalInfo.address    = data["ADDRESS"].toString();
    m_generalInfo.phone      = data["PHONE"].toString();
    m_generalInfo.isActive   = data["IS_ACTIVE"].toBool();
    m_generalInfo.isWork     = data["IS_WORK"].toBool();
    m_generalInfo.latitude   = data["LATITUDE"].toDouble();
    m_generalInfo.longitude  = data["LONGITUDE"].toDouble();

    emit generalInfoReady();
}
