#ifndef PINGDIALOG_H
#define PINGDIALOG_H
#include "workplacedata.h"
#include "pingmodel.h"

#include <QDialog>
#include <QCloseEvent> // Додано для QCloseEvent

namespace Ui {
class PingDialog;
}

class PingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PingDialog(WorkplaceData *wk, QWidget *parent = nullptr);
    ~PingDialog();

private slots:
    void slotGetPingString(const QString& output); // Змінили на QString
    void on_toolButtonCopyHost_clicked();
    void on_buttonBox_rejected();

private:
    void createUI();

private:
    Ui::PingDialog *ui;
    WorkplaceData *curWorplace;
    PingModel *modelPing;

protected:
    void closeEvent(QCloseEvent *event) override; // Додано override
};

#endif // PINGDIALOG_H
