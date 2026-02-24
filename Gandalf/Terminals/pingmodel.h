#ifndef PINGMODEL_H
#define PINGMODEL_H

#include <QObject>
#include <QProcess>

class PingModel : public QObject
{
    Q_OBJECT
public:
    explicit PingModel(QObject *parent = nullptr);
    ~PingModel();

    // Передаємо хост по значенню (точніше, за константним посиланням), ніяких вказівників!
    void start_command(const QString& host);
    void stop_command();
    bool is_running() const;

signals:
    // Відправляємо вже готовий рядок, щоб не мучити UI кодуваннями
    void signalSendOutPing(const QString& output);

private slots:
    void readResult();

private:
    QProcess *ping;
    bool running;
};

#endif // PINGMODEL_H
