#include "AppParams.h"

AppParams::AppParams(QObject *parent)
    : QObject{parent}
{
    // Тут можна завантажувати параметри з файлу або інших джерел
}

AppParams::~AppParams() = default;

AppParams& AppParams::instance()
{
    static AppParams appParams;
    return appParams;
}
