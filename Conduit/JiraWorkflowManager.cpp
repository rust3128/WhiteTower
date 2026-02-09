#include "JiraWorkflowManager.h"
#include "../Oracle/Logger.h"

JiraWorkflowManager::JiraWorkflowManager(JiraClient* client, QObject *parent)
    : QObject(parent), m_client(client)
{
}

bool JiraWorkflowManager::performSafeTransition(const QString& baseUrl, const QString& issueKey,
                                                const QString& userToken, const QString& actionType,
                                                const QString& resolutionMethod, const QString& comment,
                                                const QString& timeSpent, QString& outError)
{
    logInfo() << "JiraWorkflowManager: Starting SMART transition for" << issueKey << "Action:" << actionType;

    // ---------------------------------------------------------
    // ЕТАП 1: РОЗВІДКА (GET Metadata)
    // ---------------------------------------------------------
    QJsonObject meta = fetchTransitionsMeta(baseUrl, issueKey, userToken, outError);
    if (meta.isEmpty()) return false;

    // ---------------------------------------------------------
    // ЕТАП 2: ПОШУК ПЕРЕХОДУ (Find Transition ID)
    // ---------------------------------------------------------
    QStringList searchWords;
    if (actionType == "close") {
        // Слова-маркери для закриття
        searchWords << "Виконано" << "Done" << "Resolve" << "Close" << "Закрити" << "Закрито" << "Вирішено";
    } else {
        // Слова-маркери для відхилення
        searchWords << "Скасувати" << "Cancel" << "Reject" << "Відхилити" << "Declined" << "Скасовано";
    }

    QString transitionName;
    QString transitionId = findTransitionId(meta, searchWords, transitionName);

    if (transitionId.isEmpty()) {
        outError = QString("Не знайдено кнопку переходу для дії '%1'. Перевірте статус задачі.").arg(actionType);
        logWarning() << "Smart Transition:" << outError;
        return false;
    }

    logInfo() << "Smart Transition: Found button" << transitionName << "(ID:" << transitionId << ")";

    // ---------------------------------------------------------
    // ЕТАП 3: АНАЛІЗ ПОЛІВ ТА ЗАПОВНЕННЯ (Construct Payload)
    // ---------------------------------------------------------

    // Знаходимо об'єкт переходу в JSON
    QJsonObject transitionObj;
    QJsonArray transitions = meta["transitions"].toArray();
    for (const auto& val : transitions) {
        if (val.toObject()["id"].toString() == transitionId) {
            transitionObj = val.toObject();
            break;
        }
    }

    QJsonObject fieldsDefinition = transitionObj["fields"].toObject();
    QJsonObject fieldsPayload;
    QJsonObject updatePayload;

    // Проходимось по всіх полях, які вимагає (або пропонує) цей перехід
    for (auto it = fieldsDefinition.begin(); it != fieldsDefinition.end(); ++it) {
        QString fieldKey = it.key();             // напр. "resolution" або "customfield_13102"
        QJsonObject fieldSchema = it.value().toObject();
        bool isRequired = fieldSchema["required"].toBool();
        QString fieldName = fieldSchema["name"].toString().toLower(); // напр. "спосіб вирішення"

        // А. РЕЗОЛЮЦІЯ (Resolution)
        if (fieldKey == "resolution") {
            QStringList resKeywords;
            if (actionType == "close") resKeywords << "Готово" << "Done" << "Fixed" << "Виконано";
            else resKeywords << "Відхилено" << "Скасовано" << "Won't Do" << "Rejected" << "Cancel";

            QString resOptionId = findOptionId(fieldSchema, resKeywords);
            if (!resOptionId.isEmpty()) {
                fieldsPayload["resolution"] = QJsonObject{{"id", resOptionId}};
            } else if (isRequired) {
                // Якщо обов'язково, але ми не знайшли потрібну опцію - беремо першу-ліпшу (аварійний варіант)
                // Або краще повертаємо помилку. Тут спробуємо взяти "10000" як дефолт.
                fieldsPayload["resolution"] = QJsonObject{{"id", "10000"}};
            }
        }

        // Б. СПОСІБ ВИРІШЕННЯ (Method) - шукаємо за назвою поля, а не ID!
        else if (fieldName.contains("спосіб") || fieldName.contains("method") || fieldName.contains("вирішення")) {
            QStringList methodKeywords;
            if (resolutionMethod == "visit") methodKeywords << "Візит" << "Visit" << "Виїзд";
            else methodKeywords << "Віддалено" << "Remote" << "Телефон";

            QString methodOptionId = findOptionId(fieldSchema, methodKeywords);
            if (!methodOptionId.isEmpty()) {
                fieldsPayload[fieldKey] = QJsonObject{{"id", methodOptionId}}; // Використовуємо динамічний ID поля!
            } else if (isRequired) {
                outError = QString("Jira вимагає поле '%1', але я не знайшов потрібної опції.").arg(fieldSchema["name"].toString());
                return false; // СТОП! Не відправляємо, бо буде помилка.
            }
        }

        // В. ІНШІ ОБОВ'ЯЗКОВІ ПОЛЯ (Safety Valve)
        else if (isRequired && fieldKey != "comment" && fieldKey != "worklog" && fieldKey != "assignee") {
            // Якщо ми зустріли якесь required поле, про яке не знаємо -> зупиняємось.
            outError = QString("Автоматичне закриття неможливе. Jira вимагає заповнити поле: '%1'.").arg(fieldSchema["name"].toString());
            logCritical() << "Safety Stop:" << outError << "Key:" << fieldKey;
            return false;
        }
    }

    // ---------------------------------------------------------
    // ЕТАП 4: ФОРМУВАННЯ ПАКЕТУ (Update Block)
    // ---------------------------------------------------------

    // Коментар
    if (!comment.isEmpty()) {
        QJsonArray commentBody;
        commentBody.append(QJsonObject{{"add", QJsonObject{{"body", comment}}}});
        updatePayload["comment"] = commentBody;
    }

    // Час (Worklog)
    if (!timeSpent.isEmpty()) {
        // Перевіряємо, чи підтримує перехід worklog (зазвичай так)
        QJsonArray worklogBody;
        worklogBody.append(QJsonObject{{"add", QJsonObject{{"timeSpent", timeSpent}}}});
        updatePayload["worklog"] = worklogBody;
    }

    // Збираємо фінальний JSON
    QJsonObject finalPayload;
    finalPayload["transition"] = QJsonObject{{"id", transitionId}};

    if (!fieldsPayload.isEmpty()) finalPayload["fields"] = fieldsPayload;
    if (!updatePayload.isEmpty()) finalPayload["update"] = updatePayload;

    logInfo() << "Smart Transition: Sending Payload:" << QJsonDocument(finalPayload).toJson(QJsonDocument::Compact);

    // ---------------------------------------------------------
    // ЕТАП 5: ВІДПРАВКА (Execute)
    // ---------------------------------------------------------
    QString url = baseUrl + QString("/rest/api/2/issue/%1/transitions").arg(issueKey);
    return sendTransition(url, userToken, finalPayload, outError);
}

// --- ДОПОМІЖНІ МЕТОДИ ---

QJsonObject JiraWorkflowManager::fetchTransitionsMeta(const QString& baseUrl, const QString& issueKey, const QString& userToken, QString& outError)
{
    // 1. Формуємо URL запиту
    // Нам обов'язково треба параметр ?expand=transitions.fields, щоб Jira показала, які поля обов'язкові
    QUrl url(baseUrl + QString("/rest/api/2/issue/%1/transitions?expand=transitions.fields").arg(issueKey));

    QNetworkRequest request(url);

    // 2. Налаштовуємо заголовки
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + userToken).toUtf8());
    request.setRawHeader("X-Atlassian-Token", "no-check");

    // 3. Налаштовуємо SSL (ігноруємо помилки сертифікатів для локальних серверів)
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::AnyProtocol);
    request.setSslConfiguration(sslConfig);

    logInfo() << "JiraWorkflowManager: Fetching meta from" << url.toString();

    // 4. Відправляємо GET запит
    // Використовуємо мережевий менеджер з JiraClient
    QNetworkReply* reply = m_client->networkManager()->get(request);

    // 5. СИНХРОННЕ ОЧІКУВАННЯ
    // Ми зупиняємо виконання цього методу тут, поки сервер не відповість.
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    // 6. Перевірка на помилки мережі
    if (reply->error() != QNetworkReply::NoError) {
        outError = "Meta Error: " + reply->errorString();

        // Читаємо тіло відповіді, там може бути детальний опис помилки від Jira
        QString body = reply->readAll();
        if (!body.isEmpty()) {
            outError += " | Body: " + body;
        }

        reply->deleteLater();
        return QJsonObject(); // Повертаємо пустий об'єкт
    }

    // 7. Читаємо дані
    QByteArray data = reply->readAll();
    reply->deleteLater();

    // 8. Парсимо JSON
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        outError = "Invalid JSON response from Jira (not an object)";
        return QJsonObject();
    }

    return doc.object();
}

QString JiraWorkflowManager::findTransitionId(const QJsonObject& meta, const QStringList& keywords, QString& outNameFound)
{
    QJsonArray transitions = meta["transitions"].toArray();
    for (const auto& tVal : transitions) {
        QJsonObject tObj = tVal.toObject();
        QString name = tObj["name"].toString();

        // Перевіряємо кожне ключове слово
        for (const QString& key : keywords) {
            if (name.contains(key, Qt::CaseInsensitive)) {
                outNameFound = name;
                return tObj["id"].toString();
            }
        }
    }
    return QString();
}

QString JiraWorkflowManager::findOptionId(const QJsonObject& fieldJson, const QStringList& keywords)
{
    QJsonArray allowedValues = fieldJson["allowedValues"].toArray();
    for (const auto& val : allowedValues) {
        QJsonObject opt = val.toObject();
        QString name = opt["name"].toString();  // Для Resolution
        if (name.isEmpty()) name = opt["value"].toString(); // Для CustomField

        for (const QString& key : keywords) {
            if (name.contains(key, Qt::CaseInsensitive)) {
                return opt["id"].toString();
            }
        }
    }
    return QString();
}

bool JiraWorkflowManager::sendTransition(const QString& url, const QString& userToken, const QJsonObject& payload, QString& outError)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + userToken).toUtf8());

    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::AnyProtocol);
    request.setSslConfiguration(sslConfig);

    QNetworkReply* reply = m_client->networkManager()->post(request, QJsonDocument(payload).toJson());

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        // Пробуємо дістати деталі
        QString body = reply->readAll();
        // Спробуємо розпарсити JSON помилку від Jira
        QJsonDocument errDoc = QJsonDocument::fromJson(body.toUtf8());
        if (errDoc.isObject() && errDoc.object().contains("errorMessages")) {
            QJsonArray msgs = errDoc.object()["errorMessages"].toArray();
            if (!msgs.isEmpty()) outError = msgs.first().toString();
            else outError = "Jira Error: " + body;
        }
        else if (errDoc.isObject() && errDoc.object().contains("errors")) {
            // Помилки полів
            QJsonObject errs = errDoc.object()["errors"].toObject();
            if (!errs.isEmpty()) outError = "Field Error: " + errs.begin().value().toString();
            else outError = "Jira Error: " + body;
        }
        else {
            outError = "Network Error: " + reply->errorString();
        }

        reply->deleteLater();
        return false;
    }

    reply->deleteLater();
    return true;
}
