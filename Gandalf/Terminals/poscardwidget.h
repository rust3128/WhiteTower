#ifndef POSCARDWIDGET_H
#define POSCARDWIDGET_H

#include <QWidget>
#include <QJsonObject>
#include <QClipboard>
#include <QApplication>

namespace Ui {
class PosCardWidget;
}

class PosCardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PosCardWidget(QWidget *parent = nullptr);
    ~PosCardWidget();

    // Метод для передачі даних у картку
    void setData(const QJsonObject &json, const QString &clientName, const QString &terminalId);

private slots:
    // Слот для кнопки "Копіювати"
    void onCopyClicked();

private:
    Ui::PosCardWidget *ui;

    // Зберігаємо дані для форматування при копіюванні
    QJsonObject m_posData;
    QString m_clientName;
    QString m_terminalId;

    void setupUI();
    void createConnections();
};

#endif // POSCARDWIDGET_H
