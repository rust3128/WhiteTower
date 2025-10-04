#ifndef CRIPTPASS_H
#define CRIPTPASS_H

#include <QString>
#include <QByteArray>

class CriptPass
{
public:
    // Статичний метод для доступу до єдиного екземпляра класу
    static CriptPass& instance();

    QString criptPass(const QString& password);
    QString decriptPass(const QString& password);
    QString cryptVNCPass(const QString& termID, const QString& pass);
    QString decryptVNCPass(const QString& pass);

private:
    CriptPass(); // Конструктор тепер приватний
    ~CriptPass() = default;

    // Забороняємо копіювання, щоб гарантувати унікальність екземпляра
    CriptPass(const CriptPass&) = delete;
    CriptPass& operator=(const CriptPass&) = delete;

    QByteArray hashKey;
    QByteArray hashIV;
};

#endif // CRIPTPASS_H
