#include "AppParams.h"

namespace {
// Ця функція тепер створює і повертає готову структуру
AppParams::CryptoConfig init_crypto_config()
{
    return {
        "sHj5B9p8aFUbJMqEzOb7fEc+ThZaeRJmh2xU8prlHVQ=", // .KEY
        "zYI9eYtZw8mHiQeaYEXwkA=="     // .IV
    };
}

QString init_test_constant()
{
    return "Hello from Oracle library!";
}
}

AppParams::AppParams(QObject *parent)
    : QObject{parent}
    // Конструктор тепер ініціалізує цілу групу параметрів одним рядком!
    , crypto(init_crypto_config())
    , TEST_CONSTANT(init_test_constant())
{
}

AppParams::~AppParams() = default;

AppParams& AppParams::instance()
{
    static AppParams appParams;
    return appParams;
}

// Додайте ці функції в кінець файлу AppParams.cpp

void AppParams::setParam(const QString& appName, const QString& key, const QVariant& value)
{
    m_scopedParams[appName][key] = value;
}

QVariant AppParams::getParam(const QString& appName, const QString& key, const QVariant& defaultValue) const
{
    // 1. Спочатку шукаємо параметр для конкретного додатку
    if (m_scopedParams.contains(appName) && m_scopedParams[appName].contains(key)) {
        return m_scopedParams[appName].value(key);
    }

    // 2. Якщо не знайшли, шукаємо такий самий параметр у глобальному контексті
    if (m_scopedParams.contains("Global") && m_scopedParams["Global"].contains(key)) {
        return m_scopedParams["Global"].value(key);
    }

    // 3. Якщо ніде немає, повертаємо значення за замовчуванням
    return defaultValue;
}
