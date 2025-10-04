#include "ConfigManager.h"
#include "CriptPass.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTextStream>
#include <iostream>

ConfigManager::ConfigManager()
    // Ініціалізуємо параметри значеннями за замовчуванням
    : m_dbHost("localhost")
    , m_dbPath("C:/database/data.fdb")
    , m_dbPort(3050) // Стандартний порт Firebird
    , m_dbUser("SYSDBA")
    , m_dbPassword("masterkey")
{
}

bool ConfigManager::load()
{
    QFile configFile(getConfigFilePath());
    if (!configFile.exists()) {
        return false;
    }

    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "Failed to open config file for reading: " << qPrintable(configFile.errorString()) << std::endl;
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll(), &parseError);
    configFile.close();

    if (parseError.error != QJsonParseError::NoError) {
        std::cerr << "Failed to parse config file: " << qPrintable(parseError.errorString()) << std::endl;
        return false;
    }

    QJsonObject rootObj = doc.object();
    if (!rootObj.contains("database") || !rootObj["database"].isObject()) {
        std::cerr << "Config file is missing 'database' object." << std::endl;
        return false;
    }

    QJsonObject dbObj = rootObj["database"].toObject();
    m_dbHost = dbObj.value("host").toString(m_dbHost);
    m_dbPath = dbObj.value("path").toString(m_dbPath);
    m_dbPort = dbObj.value("port").toInt(m_dbPort);
    m_dbUser = dbObj.value("user").toString(m_dbUser);
    // === ЗМІНЕНО: Розшифровуємо пароль після читання з файлу ===
    QString encryptedPass = dbObj.value("password").toString();
    m_dbPassword = CriptPass::instance().decriptPass(encryptedPass);

    return true;
}

bool ConfigManager::save()
{
    QString filePath = getConfigFilePath();
    QDir dir = QFileInfo(filePath).dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            std::cerr << "Failed to create config directory: " << qPrintable(dir.path()) << std::endl;
            return false;
        }
    }

    QFile configFile(filePath);
    if (!configFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        std::cerr << "Failed to open config file for writing: " << qPrintable(configFile.errorString()) << std::endl;
        return false;
    }

    QJsonObject dbObj;
    dbObj["host"] = m_dbHost;
    dbObj["path"] = m_dbPath;
    dbObj["port"] = m_dbPort;
    dbObj["user"] = m_dbUser;
    // === ЗМІНЕНО: Шифруємо пароль перед збереженням у файл ===
    dbObj["password"] = CriptPass::instance().criptPass(m_dbPassword);

    QJsonObject rootObj;
    rootObj["database"] = dbObj;

    QJsonDocument doc(rootObj);
    configFile.write(doc.toJson(QJsonDocument::Indented));
    configFile.close();

    return true;
}

void ConfigManager::createInteractively()
{
    QTextStream input(stdin);
    QTextStream output(stdout);

    output << "--- Interactive Configuration Setup ---" << Qt::endl;
    output << "Please provide the database connection details. Press Enter to use the default value." << Qt::endl;

    output << "Host [" << m_dbHost << "]: ";
    output.flush();
    QString line = input.readLine();
    if (!line.trimmed().isEmpty()) m_dbHost = line.trimmed();

    output << "Database file path [" << m_dbPath << "]: ";
    output.flush();
    line = input.readLine();
    if (!line.trimmed().isEmpty()) m_dbPath = line.trimmed();

    output << "Port [" << m_dbPort << "]: ";
    output.flush();
    line = input.readLine();
    if (!line.trimmed().isEmpty()) m_dbPort = line.toInt();

    output << "User [" << m_dbUser << "]: ";
    output.flush();
    line = input.readLine();
    if (!line.trimmed().isEmpty()) m_dbUser = line.trimmed();

    output << "Password [" << m_dbPassword << "]: ";
    output.flush();
    line = input.readLine();
    if (!line.trimmed().isEmpty()) m_dbPassword = line;

    if (save()) {
        output << "Configuration saved successfully to " << getConfigFilePath() << Qt::endl;
    } else {
        output << "Error: Failed to save configuration." << Qt::endl;
    }
}

void ConfigManager::printToConsole() const
{
    QTextStream output(stdout);
    output << "--- Current Configuration ---" << Qt::endl;
    output << "  Host: " << m_dbHost << Qt::endl;
    output << "  Path: " << m_dbPath << Qt::endl;
    output << "  Port: " << m_dbPort << Qt::endl;
    output << "  User: " << m_dbUser << Qt::endl;
    output << "  Password: " << "********" << Qt::endl; // Не виводимо пароль у консоль
    output << "---------------------------" << Qt::endl;
}

QString ConfigManager::getDbHost() const { return m_dbHost; }
QString ConfigManager::getDbPath() const { return m_dbPath; }
int ConfigManager::getDbPort() const { return m_dbPort; }
QString ConfigManager::getDbUser() const { return m_dbUser; }
QString ConfigManager::getDbPassword() const { return m_dbPassword; }

QString ConfigManager::getConfigFilePath() const
{
    // Завжди шукаємо теку Config поруч з .exe файлом
    return QCoreApplication::applicationDirPath() + "/Config/config.json";
}
