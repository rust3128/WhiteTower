#ifndef GENERALINFOWIDGET_H
#define GENERALINFOWIDGET_H

#include "StationDataContext.h"

#include <QWidget>
#include <QMenu>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QJsonArray>
#include <QJsonObject>

namespace Ui {
class GeneralInfoWidget;
}

class GeneralInfoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GeneralInfoWidget(QWidget *parent = nullptr);
    ~GeneralInfoWidget();

    // Метод для отримання даних з зовні
    void updateData(const StationDataContext::GeneralInfo &info);
    // МЕТОД: для завантаження списку кас
    void updateRROData(const QJsonArray &rroArray);
    // МЕТОД: для завантаження резервуарів
    void updateTanksData(const QJsonArray &tanksArray);

    void updateDispensersData(const QJsonArray &dispensersArray);
private slots:
    // Слот для обробки правого кліку по фрейму
    void onFrameContextMenu(const QPoint &pos);
    // Слот для кнопки "Копіювати ВСІ РРО"
    void onCopyAllRROClicked();
    //СЛОТ: для копіювання таблиці резервуарів
    void onCopyTanksClicked();
private:
    // --- Методи ініціалізації ---
    void setupUI();            // Тільки візуал (курсори, стилі, tooltip)
    void createConnections();  // Тільки connect (сигнали та слоти)

    // --- Бізнес-логіка ---
    void copyToClipboard(bool phoneOnly);
    void openMapInBrowser();
    // Зберігаємо масив кас, щоб кнопка "Копіювати ВСІ" мала до них доступ
    QJsonArray m_lastRroArray;
private:
    Ui::GeneralInfoWidget *ui;
    // Зберігаємо останні отримані дані
    StationDataContext::GeneralInfo m_lastInfo;
};

#endif // GENERALINFOWIDGET_H
