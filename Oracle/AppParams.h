#ifndef APPPARAMS_H
#define APPPARAMS_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QMap>

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

    // Метод для збереження динамічного параметра
    void setParam(const QString& appName, const QString& key, const QVariant& value);
    // Метод для отримання динамічного параметра
    QVariant getParam(const QString& appName, const QString& key, const QVariant& defaultValue = QVariant()) const;


    // 2. Тепер у нас є лише один член класу для всіх крипто-ключів
    const CryptoConfig crypto;

    // Інші константи залишаються як є (поки що)
    const QString TEST_CONSTANT;

private:
    explicit AppParams(QObject *parent = nullptr);
    ~AppParams() override;

    AppParams(const AppParams&) = delete;
    AppParams& operator=(const AppParams&) = delete;

private:
    // Контейнер для зберігання параметрів, завантажених з БД
    QMap<QString, QVariantMap> m_scopedParams;
};

#endif // APPPARAMS_H
