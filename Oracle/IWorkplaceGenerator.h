#ifndef IWORKPLACEGENERATOR_H
#define IWORKPLACEGENERATOR_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// Це суто віртуальний клас (інтерфейс)
class IWorkplaceGenerator
{
public:
    virtual ~IWorkplaceGenerator() = default;

    // Метод, який повинні реалізувати всі нащадки
    virtual QJsonArray generate(int clientId, int objectId, int terminalId) = 0;
};

#endif // IWORKPLACEGENERATOR_H
