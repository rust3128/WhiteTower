#ifndef APPPARAMS_H
#define APPPARAMS_H

#include <QObject>
#include <QVariant>

class AppParams : public QObject
{
    Q_OBJECT

public:
    static AppParams& instance();

    // Додаємо константу
    const QString TEST_CONSTANT = "Hello from Oracle library!";

private:
    explicit AppParams(QObject *parent = nullptr);
    ~AppParams() override;

    // Запобігаємо копіюванню
    AppParams(const AppParams&) = delete;
    AppParams& operator=(const AppParams&) = delete;
};

#endif // APPPARAMS_H
