#include "objectslistdialog.h"
#include "ui_objectslistdialog.h"
#include <QMessageBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardItemModel>
#include <QTimer>

ObjectsListDialog::ObjectsListDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ObjectsListDialog)
{
    ui->setupUi(this);
    setWindowTitle("Довідник об'єктів");

    // Таймер для відкладеного пошуку, щоб не навантажувати сервер
    m_searchDebounceTimer = new QTimer(this);
    m_searchDebounceTimer->setSingleShot(true);
    m_searchDebounceTimer->setInterval(300); // 300 мс


    setupModel();
    createConnections();
    loadFiltersData(); // Завантажуємо дані для випадаючих списків

    onFilterChanged(); // Робимо початковий запит, щоб заповнити таблицю

    // Додайте це в кінець конструктора
    ui->comboBoxClient->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    ui->comboBoxClient->setMinimumContentsLength(20); // Мінімально 15 символів видно

    ui->comboBoxRegion->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    ui->comboBoxRegion->setMinimumContentsLength(20);
}

ObjectsListDialog::~ObjectsListDialog()
{
    delete ui;
}

void ObjectsListDialog::setupModel()
{
    // ... (код цього методу залишається без змін) ...
    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({
        "Клієнт", "ID терміналу", "Назва АЗС", "Адреса", "Регіон",
        "Активний", "В роботі"
    });
    ui->tableViewObjects->setModel(m_model);
    ui->tableViewObjects->hideColumn(3);
}

void ObjectsListDialog::loadFiltersData()
{
    // Перевикористовуємо існуючий метод для клієнтів
    ApiClient::instance().fetchAllClients();
    // Використовуємо новий метод для регіонів
    ApiClient::instance().fetchRegionsList();
}

void ObjectsListDialog::adjustComboWidth(QComboBox* combo)
{
    if (!combo) return;

    int maxTextWidth = 0;
    QFontMetrics fm(combo->font());

    // 1. Шукаємо найдовший текст
    for (int i = 0; i < combo->count(); ++i) {
        int itemWidth = fm.horizontalAdvance(combo->itemText(i));
        if (itemWidth > maxTextWidth) {
            maxTextWidth = itemWidth;
        }
    }

    // Додаємо запас (іконка стрілки + рамка + скролбар)
    int popupWidth = maxTextWidth + 40;

    // 2. ВАЖЛИВО: Встановлюємо ширину ВИПАДАЮЧОГО СПИСКУ окремо
    // Це дозволяє бачити повні назви, коли список відкритий
    combo->view()->setMinimumWidth(popupWidth);

    // 3. Встановлюємо ширину САМОЇ КНОПКИ
    // Обмежуємо максимум, щоб вікно не розтягувалось на весь екран (наприклад, 400px)
    int buttonWidth = qMin(popupWidth, 400);

    combo->setMinimumWidth(buttonWidth);

    // 4. ВАЖЛИВО: Змінюємо політику розміру, щоб Layout не стискав віджет
    QSizePolicy policy = combo->sizePolicy();
    policy.setHorizontalPolicy(QSizePolicy::Minimum); // "Я не можу бути меншим за minWidth"
    combo->setSizePolicy(policy);
}

void ObjectsListDialog::createConnections()
{
    // Відповіді від сервера
    connect(&ApiClient::instance(), &ApiClient::objectsFetched, this, &ObjectsListDialog::onObjectsReceived);
    connect(&ApiClient::instance(), &ApiClient::clientsFetched, this, &ObjectsListDialog::onClientsReceived);
    connect(&ApiClient::instance(), &ApiClient::regionsListFetched, this, &ObjectsListDialog::onRegionsReceived);

    // Помилки (можна додати окремі обробники, якщо потрібно)
    connect(&ApiClient::instance(), &ApiClient::objectsFetchFailed, this, [&](const ApiError& error){
        QMessageBox::critical(this, "Помилка", "Не вдалося завантажити список об'єктів:\n" + error.errorString);
    });

    // === З'єднання віджетів фільтрації з нашим слотом ===
    connect(ui->comboBoxClient, &QComboBox::currentIndexChanged, this, &ObjectsListDialog::onFilterChanged);
    connect(ui->comboBoxRegion, &QComboBox::currentIndexChanged, this, &ObjectsListDialog::onFilterChanged);

    // Додаємо з'єднання для нового поля. Воно теж використовує таймер.
    connect(ui->lineEditTerminalId, &QLineEdit::textChanged, m_searchDebounceTimer, QOverload<>::of(&QTimer::start));
    // Для пошуку використовуємо таймер
    connect(ui->lineEditSearch, &QLineEdit::textChanged, m_searchDebounceTimer, QOverload<>::of(&QTimer::start));
    connect(m_searchDebounceTimer, &QTimer::timeout, this, &ObjectsListDialog::onFilterChanged);
}

void ObjectsListDialog::onClientsReceived(const QJsonArray &clients)
{
    ui->comboBoxClient->clear();
    ui->comboBoxClient->addItem("Всі клієнти", -1); // "Нульовий" елемент
    for (const auto& val : clients) {
        QJsonObject obj = val.toObject();
        ui->comboBoxClient->addItem(obj["client_name"].toString(), obj["client_id"].toInt());
    }
    adjustComboWidth(ui->comboBoxClient);
}

void ObjectsListDialog::onRegionsReceived(const QStringList &regions)
{
    ui->comboBoxRegion->clear();
    ui->comboBoxRegion->addItem("Всі регіони");
    ui->comboBoxRegion->addItems(regions);

    adjustComboWidth(ui->comboBoxClient);
}

void ObjectsListDialog::onFilterChanged()
{
    QVariantMap filters;

    // 1. Збираємо дані з comboBoxClient
    int clientId = ui->comboBoxClient->currentData().toInt();
    if (clientId > 0) {
        filters["clientId"] = clientId;
    }

    // 2. Збираємо дані з comboBoxRegion
    QString region = ui->comboBoxRegion->currentText();
    if (ui->comboBoxRegion->currentIndex() > 0) { // Ігноруємо "Всі регіони"
        filters["region"] = region;
    }

    // 3. Збираємо дані з lineEditSearch
    QString searchText = ui->lineEditSearch->text().trimmed();
    if (!searchText.isEmpty()) {
        filters["search"] = searchText;
    }

    // 4. Збираємо дані з lineEditTerminalId (НОВИЙ КОД)
    QString terminalIdText = ui->lineEditTerminalId->text().trimmed();
    if (!terminalIdText.isEmpty()) {
        bool ok;
        int terminalId = terminalIdText.toInt(&ok);
        if (ok) { // Додаємо фільтр, тільки якщо це дійсно число
            filters["terminalId"] = terminalId;
        }
    }

    // 5. Викликаємо API з зібраними фільтрами
    ApiClient::instance().fetchObjects(filters);
}

void ObjectsListDialog::onObjectsReceived(const QJsonArray &objects)
{
    // ... (код цього методу залишається без змін) ...
    m_model->removeRows(0, m_model->rowCount());
    for (const QJsonValue& value : objects) {
        QJsonObject obj = value.toObject();
        QList<QStandardItem*> rowItems;
        rowItems.append(new QStandardItem(obj["client_name"].toString()));
        rowItems.append(new QStandardItem(QString::number(obj["terminal_id"].toInt())));
        rowItems.append(new QStandardItem(obj["name"].toString()));
        rowItems.append(new QStandardItem(obj["address"].toString()));
        rowItems.append(new QStandardItem(obj["region_name"].toString()));

        bool isActive = (obj["is_active"].toInt(0) == 1);
        bool isWork = (obj["is_work"].toInt(0) == 1);

        auto itemIsActive = new QStandardItem();
        itemIsActive->setCheckable(true);
        itemIsActive->setCheckState(isActive ? Qt::Checked : Qt::Unchecked);
        itemIsActive->setEditable(false);
        itemIsActive->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        rowItems.append(itemIsActive);

        auto itemIsWork = new QStandardItem();
        itemIsWork->setCheckable(true);
        itemIsWork->setCheckState(isWork ? Qt::Checked : Qt::Unchecked);
        itemIsWork->setEditable(false);
        rowItems.append(itemIsWork);
        itemIsWork->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_model->appendRow(rowItems);
    }
    ui->tableViewObjects->resizeColumnsToContents();
}
