#include "pingdialog.h"
#include "ui_pingdialog.h"
//#include "../AppParams/appparams.h"
#include <QClipboard>
#include <QGuiApplication>

PingDialog::PingDialog(WorkplaceData *wk, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PingDialog),
    curWorplace(wk)
{
    ui->setupUi(this);

    // ВАЖЛИВО: Оскільки вікно немодальне, воно має самознищитись при закритті!
    setAttribute(Qt::WA_DeleteOnClose);

    // Робимо діалог parent-ом для modelPing, щоб він точно видалився разом з вікном
    modelPing = new PingModel(this);

    connect(modelPing, &PingModel::signalSendOutPing, this, &PingDialog::slotGetPingString);

    createUI();

    // Запускаємо процес (передаємо строку напряму)
    modelPing->start_command(curWorplace->getIpAdr());
}

PingDialog::~PingDialog()
{
    // modelPing видалиться автоматично, бо ми передали 'this' у конструктор
    delete ui;
}

void PingDialog::createUI()
{
    QString strTitle = "АЗС " + QString::number(curWorplace->getTerminalID());
    strTitle += " " + curWorplace->getDisplayName();

    this->setWindowTitle(strTitle);
    ui->lineEditHost->setText(curWorplace->getIpAdr());
}

// Приймаємо вже готовий рядок, декодування робить модель
void PingDialog::slotGetPingString(const QString& pStr)
{
    ui->plainTextEditPing->appendPlainText(pStr);
}

void PingDialog::on_toolButtonCopyHost_clicked()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(ui->lineEditHost->text());
}

void PingDialog::on_buttonBox_rejected()
{
    modelPing->stop_command();
    this->reject();
}

void PingDialog::closeEvent(QCloseEvent *event)
{
    modelPing->stop_command();
    event->accept();
}
