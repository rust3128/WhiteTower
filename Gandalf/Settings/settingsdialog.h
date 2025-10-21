#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include "Oracle/ApiClient.h"

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();
private slots:
    // Слот, який викликається при натисканні кнопки "Зберегти"
    void saveSettings();

    // Слоти для обробки відповіді від сервера
    void onSettingsUpdateSuccess();
    void onSettingsUpdateFailed(const ApiError& error);
private:
    // Приватні методи для чистої структури
    void createConnections();
    void loadSettings();
private:
    Ui::SettingsDialog *ui;
};

#endif // SETTINGSDIALOG_H
