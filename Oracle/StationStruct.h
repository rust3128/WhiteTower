#ifndef STATIONSTRUCT_H
#define STATIONSTRUCT_H

#include <QString>
#include <QMetaType>

struct StationStruct {
    int objectId;
    int terminalId;
    QString clientName; // З таблиці CLIENTS
    QString address;
    bool isActive;      // IS_ACTIVE
    bool isWork;        // IS_WORK
};

Q_DECLARE_METATYPE(StationStruct)

#endif // STATIONSTRUCT_H
