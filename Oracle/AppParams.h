#ifndef APPPARAMS_H
#define APPPARAMS_H

#include <QObject>
#include <QString>

class AppParams : public QObject
{
    Q_OBJECT

public:
    static AppParams& instance();

    // 1. Оголошуємо структуру для налаштувань шифрування
    struct CryptoConfig {
        const QString KEY;
        const QString IV;
    };

    // 2. Тепер у нас є лише один член класу для всіх крипто-ключів
    const CryptoConfig crypto;

    // Інші константи залишаються як є (поки що)
    const QString TEST_CONSTANT;

private:
    explicit AppParams(QObject *parent = nullptr);
    ~AppParams() override;

    AppParams(const AppParams&) = delete;
    AppParams& operator=(const AppParams&) = delete;
};

#endif // APPPARAMS_H
