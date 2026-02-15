#ifndef STATIONDATACONTEXT_H
#define STATIONDATACONTEXT_H

#include <QObject>
#include <QJsonObject>
#include <QString>

class StationDataContext : public QObject {
    Q_OBJECT
public:
    explicit StationDataContext(int objectId, QObject *parent = nullptr);

    int getObjectId() const { return m_objectId; }

    // Структура для збереження загальної інформації про АЗС
    struct GeneralInfo {
        int clientId = 0;
        int terminalId = 0;
        QString clientName;
        QString address;
        QString phone;
        bool isActive = false;
        bool isWork = false;
        double latitude = 0.0;
        double longitude = 0.0;
    };

    const GeneralInfo& getGeneralInfo() const { return m_generalInfo; }

    // Метод для запуску завантаження даних з сервера
    void fetchGeneralInfo();

signals:
    // Сигнал: Дані успішно завантажені та готові до використання
    void generalInfoReady();
    // Сигнал: Сталася помилка при завантаженні
    void dataFetchError(const QString &error);

private slots:
    // Внутрішній слот для обробки відповіді від ApiClient
    void onApiDataReceived(int fetchedObjectId, const QJsonObject &data);

private:
    int m_objectId;
    GeneralInfo m_generalInfo;
};

#endif // STATIONDATACONTEXT_H
