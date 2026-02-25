#ifndef WORKPLACEDATA_H
#define WORKPLACEDATA_H

#include <QString>
#include <QJsonObject>

class WorkplaceData
{
public:
    WorkplaceData();

    // --- Геттери та Сеттери ---
    int getWorkplaceID() const;
    void setWorkplaceID(int newWorkplaceID);

    int getVersionType() const;
    void setVersionType(int newVersionType);

    int getPosID() const;
    void setPosID(int newPosID);

    QString getIpAdr() const;
    void setIpAdr(const QString &newIpAdr);

    QString getPassVNC() const;
    void setPassVNC(const QString &newPassVNC);

    int getPortVNC() const;
    void setPortVNC(int newPortVNC);

    int getTerminalID() const;
    void setTerminalID(int newTerminalID);

    bool getIsReachable() const;
    void setIsReachable(bool newIsReachable);

    QString getVncPath() const;
    void setVncPath(const QString &newVncPath);

    // --- "Розумні" методи для UI ---

    // Генерує красиву назву для картки
    QString getDisplayName() const;

    // --- Допоміжні методи ---
    // Парсить JSON, який пришле наш WebServer
    static WorkplaceData fromJson(const QJsonObject& json);

private:
    int m_workplaceID;
    int m_versionType;
    int m_posID;
    QString m_ipAdr;
    QString m_passVNC;
    int m_portVNC;
    int m_terminalID;
    bool m_isReachable;
    QString m_vncPath;
};

#endif // WORKPLACEDATA_H
