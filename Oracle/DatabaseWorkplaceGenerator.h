#ifndef DATABASEWORKPLACEGENERATOR_H
#define DATABASEWORKPLACEGENERATOR_H

#include "IWorkplaceGenerator.h"

class DatabaseWorkplaceGenerator : public IWorkplaceGenerator
{
public:
    DatabaseWorkplaceGenerator() = default;

    // Реалізація нашого контракту
    QJsonArray generate(int clientId, int objectId, int terminalId) override;
};

#endif // DATABASEWORKPLACEGENERATOR_H
