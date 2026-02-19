#ifndef WORKPLACEWIDGET_H
#define WORKPLACEWIDGET_H

#include <QWidget>

namespace Ui {
class WorkplaceWidget;
}

class WorkplaceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WorkplaceWidget(QWidget *parent = nullptr);
    ~WorkplaceWidget();

    // Метод-заглушка для встановлення тексту
    void setWorkplaceData(const QString &name, const QString &ip);

private:
    Ui::WorkplaceWidget *ui;
};

#endif // WORKPLACEWIDGET_H
