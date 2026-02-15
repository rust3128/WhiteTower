#include "mainwindow.h"
#include "Oracle/User.h"
#include "version.h"
#include "./ui_mainwindow.h"
#include "Users/userlistdialog.h"
#include "Clients/clientslistdialog.h"
#include "Settings/settingsdialog.h"
#include "Settings/exporttasksdialog.h"
#include "Clients/objectslistdialog.h"
#include "Oracle/SessionManager.h"
#include "Oracle/Logger.h"
#include "Clients/syncstatusdialog.h"
#include "Oracle/ApiClient.h"
#include "Oracle/AppParams.h"
#include "Clients/SyncManager.h"
#include "Terminals/StationSearchWidget.h"
#include "Terminals/StationDataContext.h"
#include "Terminals/generalinfowidget.h"


#include <QMessageBox>
#include <QTimer>
#include <QDateTime>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 1. Базові налаштування вікна
    QString windowTitle = QString("Gandalf %1").arg(PROJECT_VERSION_STR);
    setWindowTitle(windowTitle);

    // 2. Читання налаштувань (це можна залишити тут або винести в initSettings)
    m_syncPeriodDays = AppParams::instance().getParam("Gandalf", "SyncPeriodDays", 1).toInt();
    if (m_syncPeriodDays <= 0) m_syncPeriodDays = 1;

    // 3. Ініціалізація компонентів UI
    setupStationSearch();

    // 4. Запуск фонових задач
    startStartupTimers();

    setupUI();
    createConnections();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUI()
{
    // 1. Вмикаємо хрестики
    ui->tabWidgetMain->setTabsClosable(true);

    // 2. Вимикаємо DocumentMode (бо він глючив), будемо малювати все самі CSS-ом
    ui->tabWidgetMain->setDocumentMode(false);

    // 3. Стиль "Google Chrome" адаптований під ручне малювання
    QString style = R"(
        /* --- Рамка контенту (Pane) --- */
        QTabWidget::pane {
            border: 1px solid #dadce0; /* Світло-сіра рамка Google */
            background: white;
            top: -1px; /* Підтягуємо вгору, щоб активна вкладка перекрила лінію */
        }

        /* --- Смуга вкладок --- */
        QTabWidget::tab-bar {
            left: 8px; /* Відступ першої вкладки */
        }

        /* --- НЕАКТИВНА Вкладка --- */
        QTabBar::tab {
            background: #f1f3f4;       /* Сірий фон */
            color: #5f6368;            /* Сірий текст */
            border: 1px solid #dadce0; /* Рамка */

            /* Округлення зверху */
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;

            min-width: 140px;
            padding: 8px 16px;
            margin-right: -1px; /* Щоб рамки злипалися */
        }

        /* --- АКТИВНА Вкладка --- */
        QTabBar::tab:selected {
            background: white;
            color: #1a73e8;            /* Google Blue */
            font-weight: bold;

            /* Головний трюк: нижня рамка біла, щоб злитися з фоном */
            border-bottom: 1px solid white;
            border-top: 1px solid #dadce0;
            border-left: 1px solid #dadce0;
            border-right: 1px solid #dadce0;
        }

        /* --- При наведенні --- */
        QTabBar::tab:hover:!selected {
            background: #e8eaed;
            color: #202124;
        }

    )";

    ui->tabWidgetMain->setStyleSheet(style);
}

void MainWindow::createConnections()
{
    // 1. Обробка закриття вкладки по хрестику
    connect(ui->tabWidgetMain, &QTabWidget::tabCloseRequested,
            this, &MainWindow::onTabCloseRequested);


    connect(&ApiClient::instance(), &ApiClient::stationPosDataReceived,
            this, &MainWindow::onStationPosDataReceived);
}

void MainWindow::setupStationSearch()
{
    // БІЛЬШЕ НЕ СТВОРЮЄМО ВІДЖЕТ ВРУЧНУ (new ...)
    // Він вже створений у ui->setupUi(this) і доступний як ui->searchWidget

    // Перевіряємо на всяк випадок (хоча він точно є)
    if (ui->widgetSearch) {
        // Підключаємо сигнал
        connect(ui->widgetSearch, &StationSearchWidget::objectSelected,
                this, &MainWindow::onStationSelected);

        logInfo() << "UI: Search widget connected successfully via Designer.";
    } else {
        logCritical() << "UI: CRITICAL ERROR - searchWidget not found in UI file!";
    }
}

void MainWindow::startStartupTimers()
{
    // Запускаємо перевірку синхронізації через 1 секунду
    QTimer::singleShot(1000, this, &MainWindow::checkAutoSyncNeeded);

    // Запускаємо оновлення глобальних налаштувань через 2 секунди
    QTimer::singleShot(2000, this, &MainWindow::fetchGlobalSettings);
}

void MainWindow::on_actionUsers_triggered()
{
    UserListDialog *userList = new UserListDialog();
    userList->exec();
}


void MainWindow::fetchGlobalSettings()
{
    logInfo() << "Fetching Global application settings from API...";

    // 1. Підписуємося на сигнали відповіді
    connect(&ApiClient::instance(), &ApiClient::settingsFetched,
            this, &MainWindow::onGlobalSettingsFetched,
            Qt::SingleShotConnection); // Використовуємо SingleShot, бо нам потрібно отримати дані один раз

    connect(&ApiClient::instance(), &ApiClient::settingsFetchFailed,
            this, &MainWindow::onGlobalSettingsFetchFailed,
            Qt::SingleShotConnection);

    // 2. Робимо API-запит для групи "Global"
    ApiClient::instance().fetchSettings("Global");
}

void MainWindow::onGlobalSettingsFetched(const QVariantMap& settingsMap)
{
    logInfo() << "Successfully fetched Global settings.";

    // !!! КЛЮЧОВИЙ МОМЕНТ: Кешування даних у AppParams
    AppParams::instance().setScopedParams("Global", settingsMap);

    // Оскільки налаштування оновлено, якщо діалог налаштувань відкрито,
    // ви можете оновити його, викликавши його loadSettings() (потрібен доступ до екземпляра).
    // Або, якщо він не відкритий, він просто завантажить правильні дані, коли його відкриють.
}

void MainWindow::onGlobalSettingsFetchFailed(const ApiError& error)
{
    logCritical() << "Failed to fetch Global settings. Error:" << error.errorString;
    // Тут можна додати логіку для оповіщення користувача, якщо це критично
}

void MainWindow::on_actionClients_triggered()
{
    ClientsListDialog dlg(this);
    dlg.exec();
}


void MainWindow::on_action_triggered()
{
    SettingsDialog dlg(this);
    dlg.exec();
}


void MainWindow::on_actionObjectsList_triggered()
{
    ObjectsListDialog dlg(this);
    dlg.exec();
}


void MainWindow::on_actionExportTasks_triggered()
{
    // Перевірка, чи користувач має права адміністратора
    const User* currentUser = SessionManager::instance().currentUser();
    if (!currentUser || !currentUser->isAdmin()) {
        QMessageBox::critical(this, "Доступ заборонено",
                              "У вас немає прав адміністратора для керування завданнями експорту.");
        return;
    }

    logInfo() << "Opening Export Tasks Management Dialog.";

    // Створюємо та запускаємо діалог
    ExportTasksDialog dialog(this);
    dialog.exec();
}


void MainWindow::on_actionOpenSyncMonitor_triggered()
{
    SyncStatusDialog dlg(this);

    // Відкриваємо його в модальному режимі (блокує головне вікно, поки відкритий)
    dlg.exec();
}

void MainWindow::checkAutoSyncNeeded()
{
    // Підписуємося на сигнал ОТРИМАННЯ ДАНИХ один раз
    // Важливо використати Qt::SingleShotConnection, щоб цей слот спрацював лише 1 раз,
    // а не викликався щоразу, коли ми відкриваємо діалог моніторингу.
    connect(&ApiClient::instance(), &ApiClient::dashboardDataFetched,
            this, &MainWindow::onDashboardDataForAutoSync,
            Qt::SingleShotConnection);

    // Робимо запит до API (той самий, що і для діалогу)
    ApiClient::instance().fetchDashboardData();
}

void MainWindow::onDashboardDataForAutoSync(const QJsonArray& data)
{
    logInfo() << "Checking clients for auto-sync...";
    int queuedCount = 0;
    QDateTime now = QDateTime::currentDateTime();

    for (const QJsonValue& val : data) {
        QJsonObject obj = val.toObject();
        int clientId = obj["client_id"].toInt();
        QString lastSyncStr = obj["last_sync_date"].toString();
        QString status = obj["status"].toString();

        bool needSync = false;

        // 1. Якщо ніколи не синхронізувався
        if (lastSyncStr.isEmpty()) {
            needSync = true;
        }
        else {
            // 2. Перевіряємо давність
            QDateTime lastSync = QDateTime::fromString(lastSyncStr, "yyyy-MM-dd HH:mm:ss");
            if (lastSync.isValid()) {
                // Різниця у днях
                qint64 daysDiff = lastSync.daysTo(now);
                if (daysDiff >= m_syncPeriodDays) {
                    needSync = true;
                }
            } else {
                // Дата некоректна -> синхронізуємо
                needSync = true;
            }
        }

        // Додаткова умова: Якщо останній статус FAILED -> теж можна спробувати ще раз
        if (status == "FAILED") {
            needSync = true;
        }

        if (needSync) {
            logInfo() << "Auto-sync triggered for client" << clientId;
            SyncManager::instance().queueClient(clientId);
            queuedCount++;
        }
    }

    if (queuedCount > 0) {
        // Можна показати спливаюче повідомлення у треї або статус-барі
        ui->statusbar->showMessage(QString("Автозапуск: Розпочато синхронізацію %1 клієнтів").arg(queuedCount), 10000);

        // (Опціонально) Можна автоматично відкрити монітор, якщо хочете:
        // on_actionOpenSyncMonitor_triggered();
    } else {
        logInfo() << "All clients are up to date.";
    }
}


void MainWindow::onStationSelected(int objectId)
{
    logInfo() << "MainWindow: Request to open station ID:" << objectId;

    // КРОК 1: Перевірка на дублікати
    // Якщо вкладка вже є - просто фокусуємось на ній
    int existingIndex = findTabIndexByStationId(objectId);
    if (existingIndex != -1) {
        ui->tabWidgetMain->setCurrentIndex(existingIndex);
        logInfo() << "MainWindow: Tab already exists, switching to index" << existingIndex;
        return;
    }

    // КРОК 2: Створення нової вкладки
    GeneralInfoWidget *infoWidget = new GeneralInfoWidget();

    // Зберігаємо ID як динамічну властивість віджета (щоб потім знайти його в findTabIndexByStationId)
    infoWidget->setProperty("stationId", objectId);

    // КРОК 3: Створення контексту даних
    // Робимо infoWidget батьком для context, щоб при закритті вкладки context теж видалився
    StationDataContext *context = new StationDataContext(objectId, infoWidget);

    // КРОК 4: Налаштування оновлення даних
    connect(context, &StationDataContext::generalInfoReady, this, [this, infoWidget, context]() {
        // Оновлюємо внутрішній UI віджета
        auto info = context->getGeneralInfo();
        infoWidget->updateData(info);

        // --- НОВЕ: ПОЗНАЧАЄМО ВКЛАДКУ ТА РОБИМО ЗАПИТ ---
        // Зберігаємо terminalId, щоб потім знайти цю вкладку при отриманні РРО
        infoWidget->setProperty("terminalId", info.terminalId);

        // Відправляємо запит на сервер за касами (telegramId = 0 за замовчуванням)
        ApiClient::instance().fetchStationPosData(info.clientId, info.terminalId);
        // ------------------------------------------------

        // Оновлюємо заголовок та іконку вкладки
        int index = ui->tabWidgetMain->indexOf(infoWidget);
        if (index != -1) {
            QString title = QString("%1 - %2").arg(info.terminalId).arg(info.clientName);
            if (title.length() > 25) title = title.left(22) + "...";

            ui->tabWidgetMain->setTabText(index, title);
            ui->tabWidgetMain->setTabIcon(index, drawStatusIcon(info.isActive, info.isWork));
        }
    });
    // КРОК 5: Додавання на форму та запуск
    // Додаємо з тимчасовим заголовком
    int newIndex = ui->tabWidgetMain->addTab(infoWidget, QString("Завантаження %1...").arg(objectId));
    ui->tabWidgetMain->setTabIcon(newIndex, drawStatusIcon(false, false)); // Сіра іконка поки вантажиться
    ui->tabWidgetMain->setCurrentIndex(newIndex);

    // Запускаємо отримання даних
    context->fetchGeneralInfo();
}

void MainWindow::onTabCloseRequested(int index)
{
    QWidget *targetWidget = ui->tabWidgetMain->widget(index);
    if (targetWidget) {
        // deleteLater безпечно видалить віджет і всі його діти (в т.ч. StationDataContext)
        targetWidget->deleteLater();
        ui->tabWidgetMain->removeTab(index);
    }
}

// --- ДОПОМІЖНІ МЕТОДИ ---

int MainWindow::findTabIndexByStationId(int objectId)
{
    for (int i = 0; i < ui->tabWidgetMain->count(); ++i) {
        QWidget *tab = ui->tabWidgetMain->widget(i);
        // Перевіряємо властивість, яку ми задали при створенні
        if (tab && tab->property("stationId").isValid()) {
            if (tab->property("stationId").toInt() == objectId) {
                return i;
            }
        }
    }
    return -1; // Не знайдено
}

QIcon MainWindow::drawStatusIcon(bool isActive, bool isWork)
{
    QColor color;
    if (isActive && isWork)      color = QColor(0x34A853); // Зелений
    else if (isActive && !isWork) color = QColor(0xFBBC05); // Жовтий
    else if (!isActive)           color = QColor(0xBDBDBD); /* Сірий */
    else                          color = QColor(0xEA4335); // Червоний

    QPixmap pixmap(14, 14);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(1, 1, 12, 12);

    return QIcon(pixmap);
}


void MainWindow::onStationPosDataReceived(const QJsonArray& data, int clientId, int terminalId, qint64 telegramId)
{
    // Відсікаємо запити, які робив Telegram-бот
    if (telegramId != 0) {
        return;
    }

    // Перебираємо всі відкриті вкладки
    for (int i = 0; i < ui->tabWidgetMain->count(); ++i) {
        // Пробуємо перетворити віджет вкладки на наш GeneralInfoWidget
        GeneralInfoWidget* infoWidget = qobject_cast<GeneralInfoWidget*>(ui->tabWidgetMain->widget(i));

        // Якщо це дійсно наша вкладка і в неї співпадає terminalId
        if (infoWidget && infoWidget->property("terminalId").toInt() == terminalId) {
            logInfo() << "MainWindow: Routing RRO data to tab with terminal:" << terminalId;

            // ПЕРЕДАЄМО ДАНІ У ВІДЖЕТ!
            infoWidget->updateRROData(data);
            break; // Знайшли потрібну вкладку, оновили, виходимо з циклу
        }
    }
}
