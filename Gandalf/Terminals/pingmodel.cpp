#include "pingmodel.h"
#include <QDebug>
#include <QTextCodec>

PingModel::PingModel(QObject *parent) :
    QObject(parent), running(false)
{
    ping = new QProcess(this);

    // Об'єднуємо канали, щоб бачити і помилки, і стандартний вивід
    ping->setProcessChannelMode(QProcess::MergedChannels);

    // Слухаємо дані
    connect(ping, &QProcess::readyReadStandardOutput, this, &PingModel::readResult);

    // Відслідковуємо завершення
    connect(ping, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](){ running = false; });
}

PingModel::~PingModel(){
    stop_command();
}

void PingModel::start_command(const QString& host){
    stop_command(); // На випадок, якщо вже був запущений

    if(ping){
        QStringList args;
#ifdef Q_OS_WIN
        args << "-t" << host; // Безкінечний пінг для Windows
#else
        args << host;         // Для Linux/Mac
#endif

        ping->start("ping", args);
        running = true;
    }
}

void PingModel::stop_command()
{
    if (ping->state() == QProcess::Running) {
        ping->terminate();
        if (!ping->waitForFinished(2000)) {
            ping->kill();
        }
    }
    running = false;
}

void PingModel::readResult(){
    // Читаємо ВСІ доступні рядки в циклі
    while (ping->canReadLine()) {
        QByteArray output = ping->readLine();

        QString pStr;
#ifdef Q_OS_WIN
        // Для Windows використовуємо CP866 (кодування консолі за замовчуванням)
        QTextCodec *codec = QTextCodec::codecForName("CP866");
        if (codec) {
            pStr = codec->toUnicode(output);
        } else {
            pStr = QString::fromLocal8Bit(output);
        }
#else
        // Для Linux/Mac консоль зазвичай в UTF-8
        pStr = QString::fromUtf8(output);
#endif

        // Відправляємо сигнал в UI
        emit signalSendOutPing(pStr.trimmed());
    }
}

bool PingModel::is_running() const {
    return running;
}
