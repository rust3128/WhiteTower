#ifndef LOGGER_H
#define LOGGER_H

#include <QLoggingCategory>

// Оголошуємо категорію (без реалізації)
Q_DECLARE_LOGGING_CATEGORY(logApp)

// Макроси для зручності залишаються тут
#define logDebug()    qCDebug(logApp)
#define logInfo()     qCInfo(logApp)
#define logWarning()  qCWarning(logApp)
#define logCritical() qCCritical(logApp)

// Тепер функція приймає ім'я додатку
void initLogger(const QString& appName);

// Нові функції
void preInitLogger(const QString& appName); // Для початкового налаштування
void reconfigureLoggerFilters();          // Для оновлення фільтрів

#endif // LOGGER_H
