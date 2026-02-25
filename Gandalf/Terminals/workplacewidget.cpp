#include "workplacewidget.h"
#include "ui_workplacewidget.h"

#include "pingdialog.h"
#include "Oracle/AppParams.h"
#include "Oracle/Logger.h"

#include <QTimer>
#include <QProcess>
#include <QMessageBox>
#include <QFileInfo>

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
    m_data = data;

    // Встановлюємо текстові дані
    setWorkplaceData(m_data.getDisplayName(), m_data.getIpAdr());

    // АВТОМАТИЧНО запускаємо асинхронну перевірку статусу відразу після створення!
    checkVncStatus();
}

void WorkplaceWidget::on_toolButtonPing_clicked()
{
    PingDialog *pingDlg = new PingDialog(&m_data, this);
    pingDlg->show();
}

void WorkplaceWidget::on_toolButtonRefresh_clicked()
{
    // Ручний запуск перевірки
    checkVncStatus();
}
void WorkplaceWidget::checkVncStatus()
{
    // --- 1. СТАН "ОЧІКУВАННЯ" ---
    ui->toolButtonRefresh->setEnabled(false);

    // Вимикаємо кнопку VNC під час перевірки і ставимо відповідну підказку
    ui->toolButtonVNC->setEnabled(false);
    ui->toolButtonVNC->setToolTip("Перевірка доступності...");

    ui->labelStatus->setPixmap(QPixmap(":/res/Images/waiting_icon.png"));
    ui->labelStatus->setToolTip("Перевірка доступності VNC...");
    ui->frameCard->setStyleSheet("QFrame#frameCard { background-color: #ffffff; border: 1px solid #c0c0c0; border-radius: 5px; }");

    QTcpSocket *socket = new QTcpSocket(this);

    int port = m_data.getPortVNC(); // Перевірте назву методу!
    if (port <= 0) port = 5900;

    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);

    // --- ОБ'ЄДНАНИЙ ОБРОБНИК ПОМИЛКИ ТА ТАЙМАУТУ ---
    auto handleFailure = [this, socket, timer]() {
        if (ui->toolButtonRefresh->isEnabled()) return;

        if (timer->isActive()) timer->stop();

        ui->labelStatus->setPixmap(QPixmap(":/res/Images/offline_network_icon.png"));
        ui->labelStatus->setToolTip("VNC сервер НЕ доступний!");
        ui->frameCard->setStyleSheet("QFrame#frameCard { background-color: #ffebee; border: 2px solid #ef9a9a; border-radius: 5px; }");

        ui->toolButtonRefresh->setEnabled(true);

        // ЗАЛИШАЄМО КНОПКУ ВИМКНЕНОЮ ТА ЗМІНЮЄМО ПІДКАЗКУ
        ui->toolButtonVNC->setEnabled(false);
        ui->toolButtonVNC->setToolTip("Підключення неможливе: VNC сервер недоступний");

        socket->abort();
        socket->deleteLater();
        timer->deleteLater();
    };

    // --- 2. ОБРОБНИК УСПІХУ ---
    connect(socket, &QTcpSocket::connected, this, [this, socket, timer]() {
        if (ui->toolButtonRefresh->isEnabled()) return;

        if (timer->isActive()) timer->stop();

        ui->labelStatus->setPixmap(QPixmap(":/res/Images/online_network_icon.png"));
        ui->labelStatus->setToolTip(QString("VNC сервер ДОСТУПНИЙ (Порт: %1)").arg(socket->peerPort()));
        ui->frameCard->setStyleSheet("QFrame#frameCard { background-color: #f8fff8; border: 2px solid #8fbc8f; border-radius: 5px; }");

        ui->toolButtonRefresh->setEnabled(true);

        // ВМИКАЄМО КНОПКУ ТА СТАВИМО СТАНДАРТНУ ПІДКАЗКУ
        ui->toolButtonVNC->setEnabled(true);
        ui->toolButtonVNC->setToolTip("Підключитися через VNC");

        socket->disconnectFromHost();
        socket->deleteLater();
        timer->deleteLater();
    });

    // --- 3. ПІДКЛЮЧАЄМО СИГНАЛИ ---
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(socket, &QTcpSocket::errorOccurred, this, [handleFailure](QAbstractSocket::SocketError) { handleFailure(); });
#else
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, [handleFailure](QAbstractSocket::SocketError) { handleFailure(); });
#endif

    connect(timer, &QTimer::timeout, this, handleFailure);

    // --- 4. ЗАПУСК ---
    int timeoutMs = AppParams::instance().getParam("Gandalf", "VncTimeoutMs", 1500).toInt();
    if (timeoutMs <= 0) timeoutMs = 1500;

    timer->start(timeoutMs);
    socket->connectToHost(m_data.getIpAdr(), port);
}

void WorkplaceWidget::on_toolButtonVNC_clicked()
{
    QString pathVNC = m_data.getVncPath(); // Перевірте назву методу!

    if (pathVNC.isEmpty() || !QFileInfo::exists(pathVNC)) {
        QMessageBox::warning(this, "Помилка VNC",
                             QString("Не знайдено VNC-клієнт за шляхом:\n%1\n\n"
                                     "Перевірте налаштування робочого місця.").arg(pathVNC));
        return;
    }

    QString arguments;
    // Використовуємо ваші методи для отримання даних
    QString ip = m_data.getIpAdr();
    int port = m_data.getPortVNC(); // Або m_data.getPortVnc(), як у вас названо
    if (port <= 0) port = 5900;
    QString passVNC = m_data.getPassVNC(); // Ваша розшифровка, якщо вона вже є всередині

    if (pathVNC.contains("Ultra", Qt::CaseSensitive)) {
        // UltraVNC client
        arguments = QString("%1::%2 -password %3").arg(ip).arg(port).arg(passVNC);
    } else {
        // TightVNC client
        arguments = QString("-host=%1 -port=%2 -password=%3").arg(ip).arg(port).arg(passVNC);
    }

    // ВАЖЛИВО: Не передаємо 'this' у QProcess!
    // Це дозволить VNC працювати, навіть якщо Гандальф видалить цю картку з екрана.
    QProcess *connectVNC = new QProcess();

    // ТУТ ФОРМУЄТЕ ДАНІ ДЛЯ ЛОГУВАННЯ (LogData ld = ...)
    // LogData ld;
    // ld.set...

    // Підключення слоту до сигналу finished()
    connect(connectVNC, &QProcess::finished, connectVNC, [connectVNC /*, ld*/](int exitCode, QProcess::ExitStatus exitStatus) {

        // ВАША ЛОГІКА ЗАВЕРШЕННЯ (Відключення)
        // slotFinishVNC(exitCode, exitStatus, ld);
        // Або прямий код:
        // _ld.setLogTypeID(AppParams::LOG_TYPE_DISCONNECT);
        // Logger lg(_ld);
        // lg.writeToLog();

        logInfo() << "VNC viewer finished with exit code:" << exitCode;
        logInfo() << "Exit status:" << (exitStatus == QProcess::NormalExit ? "Normal Exit" : "Crash Exit");

        // Звільняємо пам'ять об'єкта QProcess (оскільки він не має parent-а)
        connectVNC->deleteLater();
    });

    connectVNC->setReadChannel(QProcess::StandardError);

    // Використовуємо setNativeArguments (специфічно для Windows)
#ifdef Q_OS_WIN
    connectVNC->setNativeArguments(arguments);
    connectVNC->start(pathVNC);
#else
    connectVNC->start(pathVNC, arguments.split(" ")); // Кросплатформний fallback на всяк випадок
#endif

    // ВАША ЛОГІКА СТАРТУ (Підключення)
    // log.writeToLog();
}
