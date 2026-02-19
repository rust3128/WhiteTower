#include "workplacedata.h"

WorkplaceData::WorkplaceData()
    : m_workplaceID(0), m_versionType(0), m_posID(0),
    m_portVNC(5900), m_terminalID(0), m_isReachable(false)
{
}

// --- Розумні методи ---

QString WorkplaceData::getDisplayName() const
{
    switch (m_versionType) {
    case 4:
        return QString("MPosTouch PosID = %1").arg(m_posID);
    case 5:
        return QString("MPosTouchSlv PosID = %1").arg(m_posID);
    case 6:
        return QString("MPosDir");
    case 8:
        return QString("MPosFil");
    case 10:
        return QString("Server");
    default:
        return QString("Workplace (Тип %1, PosID %2)").arg(m_versionType).arg(m_posID);
    }
}

// --- Парсинг з сервера ---

WorkplaceData WorkplaceData::fromJson(const QJsonObject& json)
{
    WorkplaceData wd;

    // Ключі мають збігатися з тими, що формуватиме сервер
    wd.setWorkplaceID(json["workplace_id"].toInt());
    wd.setVersionType(json["version_type"].toInt());
    wd.setPosID(json["pos_id"].toInt());
    wd.setIpAdr(json["ip_address"].toString());
    wd.setPassVNC(json["vnc_password"].toString());

    // Якщо порт не передано, ставимо стандартний 5900
    wd.setPortVNC(json.contains("vnc_port") && json["vnc_port"].toInt() > 0
                      ? json["vnc_port"].toInt()
                      : 5900);

    wd.setTerminalID(json["terminal_id"].toInt());

    return wd;
}

// --- Стандартні Геттери та Сеттери ---

int WorkplaceData::getWorkplaceID() const { return m_workplaceID; }
void WorkplaceData::setWorkplaceID(int id) { m_workplaceID = id; }

int WorkplaceData::getVersionType() const { return m_versionType; }
void WorkplaceData::setVersionType(int type) { m_versionType = type; }

int WorkplaceData::getPosID() const { return m_posID; }
void WorkplaceData::setPosID(int id) { m_posID = id; }

QString WorkplaceData::getIpAdr() const { return m_ipAdr; }
void WorkplaceData::setIpAdr(const QString &ip) { m_ipAdr = ip; }

QString WorkplaceData::getPassVNC() const { return m_passVNC; }
void WorkplaceData::setPassVNC(const QString &pass) { m_passVNC = pass; }

int WorkplaceData::getPortVNC() const { return m_portVNC; }
void WorkplaceData::setPortVNC(int port) { m_portVNC = port; }

int WorkplaceData::getTerminalID() const { return m_terminalID; }
void WorkplaceData::setTerminalID(int id) { m_terminalID = id; }

bool WorkplaceData::getIsReachable() const { return m_isReachable; }
void WorkplaceData::setIsReachable(bool reachable) { m_isReachable = reachable; }
