#ifndef WORKPLACEWIDGET_H
#define WORKPLACEWIDGET_H

#include <QWidget>
#include "workplacedata.h" // Додаємо цей інклуд

namespace Ui {
class WorkplaceWidget;
}

class WorkplaceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WorkplaceWidget(QWidget *parent = nullptr);
    ~WorkplaceWidget();

    // Залишаємо старий метод для сумісності з тестовими даними
    void setWorkplaceData(const QString &name, const QString &ip);

    // ДОДАЄМО НОВИЙ МЕТОД: для повноцінних даних
    void setWorkplaceData(const WorkplaceData &data);

private slots:
    // ДОДАЄМО СЛОТ ДЛЯ КНОПКИ PING
    void on_toolButtonPing_clicked();

private:
    Ui::WorkplaceWidget *ui;
    WorkplaceData m_data; // Зберігаємо дані каси тут
};

#endif // WORKPLACEWIDGET_H
