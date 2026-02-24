#include "workplacewidget.h"
#include "ui_workplacewidget.h"

#include "pingdialog.h"

WorkplaceWidget::WorkplaceWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WorkplaceWidget)
{
    ui->setupUi(this);

    // Фіксуємо висоту, щоб картки не розтягувалися по вертикалі
    this->setFixedHeight(80);
}

WorkplaceWidget::~WorkplaceWidget()
{
    delete ui;
}

void WorkplaceWidget::setWorkplaceData(const QString &name, const QString &ip)
{
    ui->labelVerType->setText(name);

    // Робимо ім'я жирним для краси
    QFont font = ui->labelVerType->font();
    font.setBold(true);
    ui->labelVerType->setFont(font);

    ui->labelIP->setText(ip);
}

void WorkplaceWidget::setWorkplaceData(const WorkplaceData &data)
{
    m_data = data; // Зберігаємо повний об'єкт

    // Викликаємо ваш існуючий метод для відмальовки тексту
    setWorkplaceData(m_data.getDisplayName(), m_data.getIpAdr());
}

void WorkplaceWidget::on_toolButtonPing_clicked()
{
    // Оскільки ми передали 'this' як parent, діалог не "загубиться" в пам'яті
    PingDialog *pingDlg = new PingDialog(&m_data, this);

    // Відкриваємо вікно немодально (щоб можна було відкрити кілька пінгів одночасно)
    pingDlg->show();
}
