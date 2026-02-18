#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "Oracle/ApiClient.h"
#include "Terminals/StationDataContext.h"
#include <QMainWindow>
#include <QJsonArray>
#include <QPixmap>
#include <QPainter>

class StationSearchWidget;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
public slots:
    // Слот, який викликає віджет пошуку
    void onStationSelected(int objectId);

private slots:
    void on_actionUsers_triggered();
    void on_actionClients_triggered();

    void on_action_triggered();

    void on_actionObjectsList_triggered();

    void on_actionExportTasks_triggered();

    void on_actionOpenSyncMonitor_triggered();
    void onDashboardDataForAutoSync(const QJsonArray& data);

    // Нові слоти для завантаження Global налаштувань
    void fetchGlobalSettings();
    void onGlobalSettingsFetched(const QVariantMap& settingsMap);
    void onGlobalSettingsFetchFailed(const ApiError& error);

    // Слот для закриття вкладки (натискання на хрестик)
    void onTabCloseRequested(int index);

    // Слот, який спрацює, коли базова інформація про АЗС завантажена
    void onStationGeneralInfoReady();

    // Слот для отримання даних про РРО від сервера
    void onStationPosDataReceived(const QJsonArray& data, int clientId, int terminalId, qint64 telegramId);

    // СЛОТ для отримання даних про Резервуари
    void onStationTanksDataReceived(const QJsonArray& data, int clientId, int terminalId, qint64 telegramId);
    // СЛОТ для отримання даних про Колонки (ПРК)
    void onStationDispensersDataReceived(const QJsonArray& data, int clientId, int terminalId, qint64 telegramId);

private:
    void checkAutoSyncNeeded();
    /**
     * @brief Налаштовує віджет пошуку та додає його на форму
     */
    void setupStationSearch();

    /**
     * @brief Запускає таймери та фонові задачі після старту
     */
    void startStartupTimers();
    QIcon drawStatusIcon(bool isActive, bool isWork);

    // --- Методи ініціалізації ---
    void setupUI();             // Налаштування вигляду (кнопки, вкладки)
    void createConnections();   // Всі connect-и в одному місці

    // --- Допоміжні методи ---
    // Перевіряє, чи вже відкрита вкладка з таким ID (повертає індекс або -1)
    int findTabIndexByStationId(int objectId);
    // Відправляє всі додаткові запити (РРО, Резервуари, Колонки)
    void fetchAdditionalStationData(const StationDataContext::GeneralInfo& info);

    // Оновлює візуальну частину самої вкладки (Назва, Іконка)
    void updateStationTabAppearance(QWidget* tabWidget, const StationDataContext::GeneralInfo& info);

private:
    Ui::MainWindow *ui;
    int m_syncPeriodDays;
    StationSearchWidget *m_searchWidget;
};
#endif // MAINWINDOW_H
