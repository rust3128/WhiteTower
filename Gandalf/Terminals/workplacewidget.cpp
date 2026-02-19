#include "workplacewidget.h"
#include "ui_workplacewidget.h"

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
