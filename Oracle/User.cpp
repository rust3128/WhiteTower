#include "User.h"

#include <QJsonArray>

User::User(int id, QString login, QString fio, bool isActive, QStringList roles,
           qint64 telegramId, const QString& jiraToken, const QString& redmineToken,
           int redmineUserId) // ДОДАНО redmineUserId
    : m_id(id), m_login(login), m_fio(fio), m_isActive(isActive), m_roles(roles),
    m_telegramId(telegramId), m_jiraToken(jiraToken), m_redmineToken(redmineToken),
    m_redmineUserId(redmineUserId) // Ініціалізація нового поля
{
}

int User::id() const { return m_id; }
const QString& User::login() const { return m_login; }
const QString& User::fio() const { return m_fio; }
bool User::isActive() const { return m_isActive; }
const QStringList& User::roles() const { return m_roles; }
qint64 User::telegramId() const { return m_telegramId; }
const QString& User::jiraToken() const { return m_jiraToken; }
const QString& User::redmineToken() const { return m_redmineToken; }
int User::redmineUserId() const { return m_redmineUserId; }
void User::setRedmineUserId(int id) { m_redmineUserId = id; }

bool User::isAdmin() const
{
    return hasRole("Адміністратор");
}

bool User::hasRole(const QString& roleName) const
{
    return m_roles.contains(roleName, Qt::CaseInsensitive);
}

QJsonObject User::toJson() const
{
    QJsonObject json;
    json["user_id"] = m_id;
    json["login"] = m_login;
    json["fio"] = m_fio;
    json["is_active"] = m_isActive;
    json["roles"] = QJsonArray::fromStringList(m_roles);
    json["telegram_id"] = m_telegramId; // Додано
    json["jira_token"] = m_jiraToken;   // Додано
    json["redmine_token"] = m_redmineToken;
    json["redmine_user_id"] = m_redmineUserId;
    return json;
}

User* User::fromJson(const QJsonObject& json)
{
    if (!json.contains("user_id") || !json.contains("login"))
        return nullptr;

    int id = json["user_id"].toInt();
    QString login = json["login"].toString();
    QString fio = json["fio"].toString();
    bool isActive = json["is_active"].toBool();
    qint64 telegranId = json["telegram_id"].toInt();
    QString jiraToken = json["jira_token"].toString();
    QString redmineToken = json["redmine_token"].toString();
    int redmineUserId = json.value("redmine_user_id").toInt();

    QStringList roles;
    if (json.contains("roles") && json["roles"].isArray()) {
        for (const auto& val : json["roles"].toArray()) {
            roles.append(val.toString());
        }
    }
    return new User(id, login, fio, isActive, roles, telegranId, jiraToken, redmineToken, redmineUserId);
}
