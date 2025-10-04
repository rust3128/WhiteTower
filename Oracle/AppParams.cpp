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
