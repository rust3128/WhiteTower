#ifndef WORKPLACEGENERATORFACTORY_H
#define WORKPLACEGENERATORFACTORY_H

#include <memory>
#include <QDebug>
#include "IWorkplaceGenerator.h"
#include "DatabaseWorkplaceGenerator.h"
// В майбутньому сюди додамо: #include "UkrnaftaWorkplaceGenerator.h" і т.д.

class WorkplaceGeneratorFactory
{
public:
    // Використовуємо std::unique_ptr, щоб пам'ять очищалася автоматично
    static std::unique_ptr<IWorkplaceGenerator> createGenerator(int methodId)
    {
        switch (methodId) {
        case 3: // DATABASE
            return std::make_unique<DatabaseWorkplaceGenerator>();

            // case 1: // UkrNafta
            //     return std::make_unique<UkrnaftaWorkplaceGenerator>();

        default:
            qWarning() << "WorkplaceFactory: Unknown generation method ID:" << methodId;
            return nullptr; // Якщо метод невідомий, повертаємо порожнечу
        }
    }
};

#endif // WORKPLACEGENERATORFACTORY_H
