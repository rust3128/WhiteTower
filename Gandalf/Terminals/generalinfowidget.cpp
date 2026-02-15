#include "generalinfowidget.h"
#include "ui_generalinfowidget.h"

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
}

void GeneralInfoWidget::createConnections()
{
    // Всі connect пишемо ТІЛЬКИ ТУТ

    // Обробка правого кліку на фреймі
    connect(ui->frameTitle, &QFrame::customContextMenuRequested,
            this, &GeneralInfoWidget::onFrameContextMenu);

    // --- НАВІГАЦІЯ QStackedWidget ---
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
    ui->labelTotalRRO->setText(QString("🧾 Знайдено касових апаратів: %1").arg(rroArray.size()));

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
