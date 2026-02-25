#include "generalinfowidget.h"
#include "ui_generalinfowidget.h"

#include "poscardwidget.h"
#include "workplacewidget.h"
#include "workplacedata.h"

#include <QHeaderView>

GeneralInfoWidget::GeneralInfoWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::GeneralInfoWidget)
{
    ui->setupUi(this);

    // 1. Налаштовуємо вигляд (стилі, курсори, початковий стан)
    setupUI();

    // 2. Підключаємо сигнали (взаємодія)
    createConnections();
}

GeneralInfoWidget::~GeneralInfoWidget()
{
    delete ui;
}

// --- ІНІЦІАЛІЗАЦІЯ ---

void GeneralInfoWidget::setupUI()
{
    // Ховаємо телефон за замовчуванням
    ui->labelPhone->setVisible(false);

    // Налаштування контейнера frameTitle
    // Вмикаємо режим CustomContextMenu, щоб фрейм реагував на ПКМ
    ui->frameTitle->setContextMenuPolicy(Qt::CustomContextMenu);

    // Візуальні підказки
    ui->frameTitle->setCursor(Qt::PointingHandCursor);
    ui->frameTitle->setToolTip("Натисніть праву кнопку миші для опцій");

    // Стилізація: прозора рамка, яка стає видимою при наведенні
    ui->frameTitle->setStyleSheet(R"(
        QFrame#frameTitle {
            border: 1px solid transparent;
            border-radius: 6px;
            padding: 5px;
        }
        QFrame#frameTitle:hover {
            background-color: #f1f3f4; /* Світло-сірий */
            border: 1px solid #dadce0; /* Рамка */
        }
    )");

    if (ui->stackedWidgetInfo) { // Замініть tabWidget на назву вашого віджета вкладок
        ui->stackedWidgetInfo->setCurrentIndex(0);
    }

    // 1. Встановлюємо жорстку мінімальну ширину для лівої панелі з касами
    // 530 пікселів гарантує, що картка (505px) + можливий вертикальний скрол влізуть ідеально
    ui->scrollAreaWorplaces->setMinimumWidth(410);

    // 2. Вказуємо сплітеру пропорції (це у вас вже було)
    ui->splitter->setSizes(QList<int>{410, 800});

    // 3. Кажемо сплітеру: "При розширенні вікна ліву частину (0) не чіпай,
    // а всю додаткову ширину віддавай правій частині (1)"
    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);


    ui->scrollAreaWorplaces->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}


void GeneralInfoWidget::createConnections()
{
    // Всі connect пишемо ТІЛЬКИ ТУТ

    // Обробка правого кліку на фреймі
    connect(ui->frameTitle, &QFrame::customContextMenuRequested,
            this, &GeneralInfoWidget::onFrameContextMenu);

    // --- НАВІГАЦІЯ QStackedWidget ---
    connect(ui->toolButtonInfo, &QToolButton::clicked, this, [this](){
        ui->stackedWidgetInfo->setCurrentWidget(ui->pageSummary);
    });
    // Перемикаємо сторінки при натисканні на відповідну вкладку-кнопку
    connect(ui->toolButtonRRO, &QToolButton::clicked, this, [this](){
        ui->stackedWidgetInfo->setCurrentWidget(ui->pageRRO);
    });
    connect(ui->toolButtonPRK, &QToolButton::clicked, this, [this](){
        ui->stackedWidgetInfo->setCurrentWidget(ui->pagePRK);
    });
    connect(ui->toolButtonTanks, &QToolButton::clicked, this, [this](){
        ui->stackedWidgetInfo->setCurrentWidget(ui->pageTanks);
    });

    // --- КНОПКА КОПІЮВАТИ ВСІ РРО ---
    connect(ui->toolButtonCopyAllRRO, &QToolButton::clicked, this, &GeneralInfoWidget::onCopyAllRROClicked);

    connect(ui->btnCopyTanks, &QToolButton::clicked, this, &GeneralInfoWidget::onCopyTanksClicked);
}

// --- ЛОГІКА ДАНИХ ---

void GeneralInfoWidget::updateData(const StationDataContext::GeneralInfo &info)
{
    m_lastInfo = info;

    // --- ВИПРАВЛЕННЯ СТАТУСУ (CSS замість Emoji) ---

    // 1. Очищаємо текст (щоб не було квадратів)
    ui->labelStatusIcon->setText("");

    // 2. Фіксуємо розмір, щоб це був ідеальний квадрат
    ui->labelStatusIcon->setFixedSize(16, 16);

    // 3. Визначаємо колір
    QString color;
    QString toolTip;

    if (info.isActive && info.isWork) {
        color = "#34A853"; // Зелений
        toolTip = "Активна, Працює";
    } else if (info.isActive && !info.isWork) {
        color = "#FBBC05"; // Жовтий
        toolTip = "Активна, НЕ працює";
    } else {
        color = "#BDBDBD"; // Сірий
        toolTip = "НЕ Активна";
    }

    // 4. Малюємо кружечок через Style Sheet
    // border-radius: 8px (це половина від розміру 16px) робить коло
    ui->labelStatusIcon->setStyleSheet(QString(
                                           "QLabel { "
                                           "  background-color: %1; "
                                           "  border-radius: 8px; "
                                           "  border: none; "
                                           "}"
                                           ).arg(color));

    ui->labelStatusIcon->setToolTip(toolTip);

    // --- РЕШТА КОДУ БЕЗ ЗМІН ---
    ui->labelTitle->setText(QString("%1, АЗС %2").arg(info.clientName).arg(info.terminalId));
    ui->labelAddress->setText(QString("📍 %1").arg(info.address));

    if (!info.phone.isEmpty()) {
        ui->labelPhone->setText(QString("📞 %1").arg(info.phone));
        ui->labelPhone->setVisible(true);
    } else {
        ui->labelPhone->setVisible(false);
    }
}

// --- ЛОГІКА МЕНЮ ---

void GeneralInfoWidget::onFrameContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    // Стиль меню (опціонально)
    menu.setStyleSheet("QMenu { background: white; border: 1px solid #ccc; }"
                       "QMenu::item { padding: 5px 20px; }"
                       "QMenu::item:selected { background: #e8f0fe; color: black; }");

    // 1. Дія: Копіювати все
    QAction *actCopyAll = menu.addAction("📋 Копіювати все");
    connect(actCopyAll, &QAction::triggered, this, [this](){
        copyToClipboard(false);
    });

    // 2. Дія: Копіювати телефон (тільки якщо є)
    if (!m_lastInfo.phone.isEmpty()) {
        QAction *actCopyPhone = menu.addAction("📞 Копіювати телефон");
        connect(actCopyPhone, &QAction::triggered, this, [this](){
            copyToClipboard(true);
        });
    }

    // 3. Дія: Відкрити карту (тільки якщо є координати)
    if (m_lastInfo.latitude != 0.0 || m_lastInfo.longitude != 0.0) {
        menu.addSeparator();
        QAction *actOpenMap = menu.addAction("🗺 Відкрити карту в браузері");
        connect(actOpenMap, &QAction::triggered, this, &GeneralInfoWidget::openMapInBrowser);
    }

    // Відображаємо меню в точці кліку
    menu.exec(ui->frameTitle->mapToGlobal(pos));
}

void GeneralInfoWidget::copyToClipboard(bool phoneOnly)
{
    QClipboard *clipboard = QApplication::clipboard();

    if (phoneOnly) {
        clipboard->setText(m_lastInfo.phone);
    } else {
        QStringList lines;
        lines << QString("%1, АЗС %2").arg(m_lastInfo.clientName).arg(m_lastInfo.terminalId);
        lines << m_lastInfo.address;

        if (!m_lastInfo.phone.isEmpty()) {
            lines << m_lastInfo.phone;
        }

        if (m_lastInfo.latitude != 0.0 || m_lastInfo.longitude != 0.0) {
            // ВИПРАВЛЕНО: Використовуємо %1 та %2 для підстановки
            // Також використовуємо HTTPS і стандартний домен
            QString link = QString("АЗС на мапі: https://www.google.com/maps?q=%1,%2")
                               .arg(m_lastInfo.latitude)
                               .arg(m_lastInfo.longitude);
            lines << link;
        }
        clipboard->setText(lines.join("\n"));
    }
}

void GeneralInfoWidget::openMapInBrowser()
{
    // ВИПРАВЛЕНО: Правильний формат URL для браузера
    QString urlStr = QString("https://www.google.com/maps?q=%1,%2")
                         .arg(m_lastInfo.latitude)
                         .arg(m_lastInfo.longitude);

    QDesktopServices::openUrl(QUrl(urlStr));
}
void GeneralInfoWidget::updateRROData(const QJsonArray &rroArray)
{
    m_lastRroArray = rroArray;

    // 1. Оновлюємо лічильник у шапці
    ui->labelTotalRRO->setText(QString("<img src=':/res/Images/RRO_icon.png' width='18' height='18' align='middle'> Знайдено касових апаратів: %1").arg(rroArray.size()));

    // 2. Очищаємо старі картки (щоб вони не накопичувалися при зміні АЗС)
    // Важливо: ми не видаляємо останній елемент, бо це наша розпірка (QSpacerItem)!
    QLayoutItem *child;
    while (ui->layoutRROCards->count() > 1) {
        // Забираємо найперший елемент (нульовий)
        child = ui->layoutRROCards->takeAt(0);
        if (QWidget *widget = child->widget()) {
            widget->deleteLater();
        }
        delete child;
    }

    // 3. Додаємо нові картки
    for (int i = 0; i < rroArray.size(); ++i) {
        QJsonObject jsonObj = rroArray[i].toObject();

        PosCardWidget *card = new PosCardWidget(this);
        // Передаємо json, Назву клієнта та ID АЗС (для копіювання)
        card->setData(jsonObj, m_lastInfo.clientName, QString::number(m_lastInfo.terminalId));

        // Вставляємо картку ПЕРЕД розпіркою
        ui->layoutRROCards->insertWidget(ui->layoutRROCards->count() - 1, card);
    }
}

// --- НОВИЙ МЕТОД: КОПІЮВАННЯ ВСЬОГО СПИСКУ ---
void GeneralInfoWidget::onCopyAllRROClicked()
{
    if (m_lastRroArray.isEmpty()) return;

    QStringList lines;

    // 1. Шапка з назвою АЗС
    lines << QString("🏢 %1, АЗС %2").arg(m_lastInfo.clientName).arg(m_lastInfo.terminalId);
    lines << QString("🧾 Всього РРО: %1\n").arg(m_lastRroArray.size());

    // 2. Перебираємо масив і формуємо текст для кожної каси
    for (int i = 0; i < m_lastRroArray.size(); ++i) {
        QJsonObject obj = m_lastRroArray[i].toObject();

        int posId = obj["pos_id"].toInt();
        QString manufacturer = obj["manufacturer"].toString();
        QString model = obj["model"].toString();
        QString zn = obj["factory_number"].toString();
        QString fn = obj["tax_number"].toString();
        QString softVer = obj["version"].toString();
        QString mukVer = obj["muk_version"].toString();
        QString regDate = obj["reg_date"].toString();

        lines << QString("🧾 Каса №%1 %2-%3").arg(posId).arg(manufacturer, model);
        lines << QString("   ЗН: %1").arg(zn);
        lines << QString("   ФН: %1").arg(fn);
        lines << QString("   💻 ПО: %1 | МУК: %2").arg(softVer, mukVer);

        if (!regDate.isEmpty()) {
            lines << QString("   📅 Реєстрація: %1").arg(regDate);
        }
        lines << ""; // Порожній рядок між касами для краси
    }

    // 3. Відправляємо в буфер
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(lines.join("\n"));
}


// --- НОВИЙ МЕТОД: ЗАВАНТАЖЕННЯ РЕЗЕРВУАРІВ ---
void GeneralInfoWidget::updateTanksData(const QJsonArray &tanksArray)
{
    // 1. Оновлюємо шапку (з HTML іконкою)
    ui->labelTotalTanks->setText(QString("<img src=':/res/Images/tanks.png' width='18' height='18' align='middle'> Резервуарів: %1").arg(tanksArray.size()));

    // 2. Базові налаштування таблиці для краси та зручності
    ui->tableWidgetTanks->setRowCount(tanksArray.size()); // Встановлюємо кількість рядків
    ui->tableWidgetTanks->setEditTriggers(QAbstractItemView::NoEditTriggers); // Забороняємо редагувати текст
    ui->tableWidgetTanks->setAlternatingRowColors(true); // Ефект "зебри" для рядків
    ui->tableWidgetTanks->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); // Розтягуємо колонки по ширині

    // 3. Заповнюємо таблицю
    for (int i = 0; i < tanksArray.size(); ++i) {
        QJsonObject obj = tanksArray[i].toObject();

        // Створюємо комірки.
        // Використовуємо .toDouble(), 'f', 0 або 2 для форматування чисел (напр. 15000.50)
        QTableWidgetItem *itemNum     = new QTableWidgetItem(QString::number(obj["tank_id"].toInt()));
        QTableWidgetItem *itemFuel    = new QTableWidgetItem(obj["fuel_shortname"].toString());
        QTableWidgetItem *itemMax     = new QTableWidgetItem(QString::number(obj["max_vol"].toDouble(), 'f', 0));
        QTableWidgetItem *itemMin     = new QTableWidgetItem(QString::number(obj["min_vol"].toDouble(), 'f', 0));
        QTableWidgetItem *itemDeadMax = new QTableWidgetItem(QString::number(obj["dead_max"].toDouble(), 'f', 0));
        QTableWidgetItem *itemDeadMin = new QTableWidgetItem(QString::number(obj["dead_min"].toDouble(), 'f', 0));
        QTableWidgetItem *itemTube    = new QTableWidgetItem(QString::number(obj["tube_vol"].toDouble(), 'f', 0));

        // Вирівнюємо текст по центру комірки
        itemNum->setTextAlignment(Qt::AlignCenter);
        itemFuel->setTextAlignment(Qt::AlignCenter);
        itemMax->setTextAlignment(Qt::AlignCenter);
        itemMin->setTextAlignment(Qt::AlignCenter);
        itemDeadMax->setTextAlignment(Qt::AlignCenter);
        itemDeadMin->setTextAlignment(Qt::AlignCenter);
        itemTube->setTextAlignment(Qt::AlignCenter);

        // Вставляємо комірки у відповідний рядок (i) та колонку (0..6)
        ui->tableWidgetTanks->setItem(i, 0, itemNum);
        ui->tableWidgetTanks->setItem(i, 1, itemFuel);
        ui->tableWidgetTanks->setItem(i, 2, itemMax);
        ui->tableWidgetTanks->setItem(i, 3, itemMin);
        ui->tableWidgetTanks->setItem(i, 4, itemDeadMax);
        ui->tableWidgetTanks->setItem(i, 5, itemDeadMin);
        ui->tableWidgetTanks->setItem(i, 6, itemTube);
    }
}


void GeneralInfoWidget::onCopyTanksClicked()
{
    if (ui->tableWidgetTanks->rowCount() == 0) return;

    QStringList lines;
    lines << QString("🏢 %1, АЗС %2").arg(m_lastInfo.clientName).arg(m_lastInfo.terminalId);
    lines << QString("🛢 Резервуарів: %1\n").arg(ui->tableWidgetTanks->rowCount());

    for (int r = 0; r < ui->tableWidgetTanks->rowCount(); ++r) {
        // Безпечно дістаємо текст з кожної комірки
        QString id      = ui->tableWidgetTanks->item(r, 0) ? ui->tableWidgetTanks->item(r, 0)->text() : "?";
        QString fuel    = ui->tableWidgetTanks->item(r, 1) ? ui->tableWidgetTanks->item(r, 1)->text() : "?";
        QString maxV    = ui->tableWidgetTanks->item(r, 2) ? ui->tableWidgetTanks->item(r, 2)->text() : "0";
        QString minV    = ui->tableWidgetTanks->item(r, 3) ? ui->tableWidgetTanks->item(r, 3)->text() : "0";
        QString deadMax = ui->tableWidgetTanks->item(r, 4) ? ui->tableWidgetTanks->item(r, 4)->text() : "0";
        QString deadMin = ui->tableWidgetTanks->item(r, 5) ? ui->tableWidgetTanks->item(r, 5)->text() : "0";
        QString tube    = ui->tableWidgetTanks->item(r, 6) ? ui->tableWidgetTanks->item(r, 6)->text() : "0";

        // Формуємо красивий рядок
        lines << QString("🛢 %1 (%2) | Об'єм: %3 / %4 | Рівн:: %5 / %6 | Труба: %7")
                     .arg(id, fuel, maxV, minV, deadMax, deadMin, tube);
    }

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(lines.join("\n"));
}


void GeneralInfoWidget::updateDispensersData(const QJsonArray &dispensersArray)
{
    // Оновлюємо шапку (якщо є)
    if (ui->labelTotalPRK) {
        ui->labelTotalPRK->setText(QString("<img src=':/res/Images/prk.png' width='18' height='18' align='middle'> ПРК: %1").arg(dispensersArray.size()));
    }

    ui->treeWidgetPRK->clear();
    ui->treeWidgetPRK->setColumnCount(5);

    // --- 1. ЧИСТІ ТА ЛОГІЧНІ ЗАГОЛОВКИ ---
    QStringList headers = {"№ ПРК", "Протокол", "Порт", "Швидкість", "Адреса"};
    ui->treeWidgetPRK->setHeaderLabels(headers);
    ui->treeWidgetPRK->setAlternatingRowColors(true);

    // Перебираємо масив ПРК
    for (int i = 0; i < dispensersArray.size(); ++i) {
        QJsonObject dispObj = dispensersArray[i].toObject();

        // --- БАТЬКІВСЬКИЙ РЯДОК (Сама Колонка) ---
        QTreeWidgetItem *dispItem = new QTreeWidgetItem(ui->treeWidgetPRK);

        dispItem->setText(0, QString::number(dispObj["dispenser_id"].toInt()));
        dispItem->setText(1, dispObj["protocol_name"].toString());
        dispItem->setText(2, QString::number(dispObj["channel_port"].toInt()));
        dispItem->setText(3, QString::number(dispObj["channel_speed"].toInt()));
        dispItem->setText(4, QString::number(dispObj["net_address"].toInt()));

        QFont boldFont = dispItem->font(0);
        boldFont.setBold(true);
        for (int col = 0; col < 5; ++col) {
            dispItem->setFont(col, boldFont);
            dispItem->setBackground(col, QBrush(QColor("#F8F9FA"))); // Сірий фон
            if (col > 0) dispItem->setTextAlignment(col, Qt::AlignCenter);
        }

        // --- ДОЧІРНІ РЯДКИ (Пістолети) ---
        QJsonArray nozzlesArray = dispObj["nozzles"].toArray();
        for (int j = 0; j < nozzlesArray.size(); ++j) {
            QJsonObject nozzleObj = nozzlesArray[j].toObject();
            QTreeWidgetItem *nozzleItem = new QTreeWidgetItem(dispItem);

            // Формуємо красивий єдиний рядок, який легко читати
            QString nozzleInfo = QString("  ↳ Пістолет %1: %2 (Рез. %3)")
                                     .arg(nozzleObj["nozzle_id"].toInt())
                                     .arg(nozzleObj["fuel_shortname"].toString())
                                     .arg(nozzleObj["tank_id"].toInt());

            nozzleItem->setText(0, nozzleInfo);

            // МАГІЯ: Дозволяємо цьому тексту розтягнутися на всі колонки!
            nozzleItem->setFirstColumnSpanned(true);

            // Робимо текст пістолетів трохи іншим кольором, щоб він не зливався з ПРК
            nozzleItem->setForeground(0, QBrush(QColor("#444444")));
        }
    }

    // Вимикаємо розтягування останньої колонки
    ui->treeWidgetPRK->header()->setStretchLastSection(false);

    // Підганяємо ширину колонок під текст ПРК
    for (int col = 0; col < 5; ++col) {
        ui->treeWidgetPRK->resizeColumnToContents(col);
    }
}

// --- ДОДАЙТЕ ЦЕЙ МЕТОД В КІНЕЦЬ ФАЙЛУ ---
void GeneralInfoWidget::createTestWorkplaces()
{
    // 1. Очищаємо ліву панель (scrollAreaWorplaces) від старих карток
    QLayoutItem *child;
    while ((child = ui->verticalLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    // 2. Створюємо 4 тестові заглушки
    for (int i = 1; i <= 4; ++i) {
        WorkplaceWidget* card = new WorkplaceWidget(this);

        // Передаємо фейкові дані (Назва та IP)
        card->setWorkplaceData(QString("MPosTouch PosID = %1").arg(i),
                               QString("10.54.100.%1").arg(80 + i));

        // Додаємо картку у вертикальний список лівої панелі
        ui->verticalLayout->addWidget(card);
    }

    // 3. Додаємо "пружину" в кінець списку, щоб картки притискалися догори
    QSpacerItem* spacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    ui->verticalLayout->addItem(spacer);
}

void GeneralInfoWidget::updateWorkplacesData(const QJsonArray &workplacesArray)
{
    // 1. Очищаємо ліву панель від старих карток
    QLayoutItem *child;
    while ((child = ui->verticalLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    // --- 2. ПЕРЕВІРКА НА МАРКЕР ТЕРМІНАЛЬНОГО СЕРВЕРА ---
    if (workplacesArray.size() == 1) {
        QJsonObject firstObj = workplacesArray.first().toObject();

        if (firstObj.contains("is_terminal_only") && firstObj["is_terminal_only"].toBool() == true) {
            QLabel* infoLabel = new QLabel(this);
            QString msg = firstObj["message"].toString("Доступ лише через термінальний сервер клієнта.");
            infoLabel->setText(QString("🔒\n%1").arg(msg));
            infoLabel->setWordWrap(true);
            infoLabel->setAlignment(Qt::AlignCenter);

            infoLabel->setStyleSheet(
                "QLabel { color: #0277bd; background-color: #e1f5fe; "
                "border: 1px solid #81d4fa; border-radius: 5px; padding: 15px; margin: 10px; font-size: 13px; }"
                );

            ui->verticalLayout->addWidget(infoLabel);
            QSpacerItem* spacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
            ui->verticalLayout->addItem(spacer);
            return;
        }
    }

    // --- 3. МИТТЄВЕ СТВОРЕННЯ КАРТОК ---
    for (int i = 0; i < workplacesArray.size(); ++i) {
        QJsonObject obj = workplacesArray[i].toObject();
        WorkplaceData wd = WorkplaceData::fromJson(obj);

        WorkplaceWidget* card = new WorkplaceWidget(this);

        // Передача даних автоматично запускає checkVncStatus() всередині картки
        card->setWorkplaceData(wd);

        // Одразу додаємо на екран (вона з'явиться з пісочним годинником)
        ui->verticalLayout->addWidget(card);
    }

    // 4. Додаємо пружину
    QSpacerItem* spacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    ui->verticalLayout->addItem(spacer);
}

void GeneralInfoWidget::showWorkplacesError(const QString &errorMsg)
{
    // 1. Очищаємо ліву панель від старих карток (або анімацій завантаження)
    QLayoutItem *child;
    while ((child = ui->verticalLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    // 2. Створюємо красивий віджет для відображення помилки
    QLabel* errorLabel = new QLabel(this);
    errorLabel->setText(QString("⚠️ Не вдалося завантажити робочі місця:\n%1").arg(errorMsg));
    errorLabel->setWordWrap(true);
    errorLabel->setAlignment(Qt::AlignCenter);

    // Стилізуємо: червоний текст, блідо-червоний фон і рамка
    errorLabel->setStyleSheet(
        "QLabel { "
        "  color: #d32f2f; "
        "  background-color: #ffebee; "
        "  border: 1px solid #ef9a9a; "
        "  border-radius: 5px; "
        "  padding: 10px; "
        "  margin: 10px; "
        "}"
        );

    ui->verticalLayout->addWidget(errorLabel);

    // 3. Додаємо пружину, щоб картка помилки притиснулася догори
    QSpacerItem* spacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    ui->verticalLayout->addItem(spacer);
}


// ==========================================================
// --- МЕТОДИ ВІДОБРАЖЕННЯ ПОМИЛОК (ERROR STATES) ---
// ==========================================================

void GeneralInfoWidget::showRROError(const QString &errorMsg)
{
    // 1. Змінюємо текст у шапці
    ui->labelTotalRRO->setText("<img src=':/res/Images/RRO_icon.png' width='18' height='18' align='middle'> Знайдено касових апаратів: Помилка");

    // 2. Очищаємо старі картки (щоб вони не накопичувалися), але залишаємо пружину
    QLayoutItem *child;
    while (ui->layoutRROCards->count() > 1) {
        child = ui->layoutRROCards->takeAt(0);
        if (QWidget *widget = child->widget()) {
            widget->deleteLater();
        }
        delete child;
    }

    // 3. Створюємо картку з помилкою
    QLabel* errorLabel = new QLabel(this);
    errorLabel->setText(QString("⚠️ Не вдалося завантажити дані РРО:\n%1").arg(errorMsg));
    errorLabel->setWordWrap(true);
    errorLabel->setAlignment(Qt::AlignCenter);

    // Стилізуємо під червоне попередження
    errorLabel->setStyleSheet(
        "QLabel { "
        "  color: #d32f2f; "
        "  background-color: #ffebee; "
        "  border: 1px solid #ef9a9a; "
        "  border-radius: 5px; "
        "  padding: 10px; "
        "  margin: 10px; "
        "}"
        );

    // 4. Вставляємо картку ПЕРЕД розпіркою (на індекс 0)
    ui->layoutRROCards->insertWidget(0, errorLabel);
}

void GeneralInfoWidget::showTanksError(const QString &errorMsg)
{
    // 1. Змінюємо текст у шапці
    ui->labelTotalTanks->setText("<img src=':/res/Images/tanks.png' width='18' height='18' align='middle'> Резервуарів: Помилка");

    // 2. Очищаємо таблицю і залишаємо лише 1 рядок
    ui->tableWidgetTanks->clearContents();
    ui->tableWidgetTanks->setRowCount(1);

    // 3. Створюємо комірку з текстом помилки
    QTableWidgetItem *errorItem = new QTableWidgetItem(QString("⚠️ Не вдалося завантажити резервуари: %1").arg(errorMsg));
    errorItem->setTextAlignment(Qt::AlignCenter);
    errorItem->setForeground(QBrush(QColor("#d32f2f"))); // Червоний текст
    errorItem->setBackground(QBrush(QColor("#ffebee"))); // Блідо-червоний фон

    // 4. Вставляємо в 1-шу комірку (0,0) і наказуємо їй розтягнутися на ВСІ колонки
    ui->tableWidgetTanks->setItem(0, 0, errorItem);
    ui->tableWidgetTanks->setSpan(0, 0, 1, ui->tableWidgetTanks->columnCount());
}

void GeneralInfoWidget::showPRKError(const QString &errorMsg)
{
    // 1. Змінюємо текст у шапці
    if (ui->labelTotalPRK) {
        ui->labelTotalPRK->setText("<img src=':/res/Images/prk.png' width='18' height='18' align='middle'> ПРК: Помилка");
    }

    // 2. Очищаємо дерево
    ui->treeWidgetPRK->clear();

    // 3. Створюємо рядок для повідомлення
    QTreeWidgetItem *errorItem = new QTreeWidgetItem(ui->treeWidgetPRK);
    errorItem->setText(0, QString("⚠️ Не вдалося завантажити ПРК:\n%1").arg(errorMsg));
    errorItem->setTextAlignment(0, Qt::AlignCenter);
    errorItem->setForeground(0, QBrush(QColor("#d32f2f")));
    errorItem->setBackground(0, QBrush(QColor("#ffebee")));

    // 4. Магія QTreeWidget: розтягуємо текст першої колонки на всю ширину віджета
    errorItem->setFirstColumnSpanned(true);
}

// void GeneralInfoWidget::onWorkplaceStatusChecked()
// {
//     m_pendingChecksCount--;

//     // Якщо всі картки відзвітували (лічильник дійшов до 0)
//     if (m_pendingChecksCount <= 0) {

//         // 1. Прибираємо напис "Шукаємо робочі місця..."
//         QLayoutItem *child;
//         while ((child = ui->verticalLayout->takeAt(0)) != nullptr) {
//             if (child->widget()) child->widget()->deleteLater();
//             delete child;
//         }

//         // 2. Додаємо всі готові, перевірені та розфарбовані картки на екран
//         for (WorkplaceWidget* card : m_pendingWorkplaceWidgets) {
//             ui->verticalLayout->addWidget(card);
//         }

//         m_pendingWorkplaceWidgets.clear(); // Очищаємо тимчасовий список

//         // 3. Додаємо пружину внизу
//         QSpacerItem* spacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
//         ui->verticalLayout->addItem(spacer);
//     }
// }
