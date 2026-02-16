#include "poscardwidget.h"
#include "ui_poscardwidget.h"

PosCardWidget::PosCardWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PosCardWidget)
{
    ui->setupUi(this);

    setupUI();
    createConnections();
}

PosCardWidget::~PosCardWidget()
{
    delete ui;
}

void PosCardWidget::setupUI()
{
    // Стилізуємо QFrame (робимо картку красивою)
    ui->frame->setStyleSheet(R"(
        QFrame#frame {
            background-color: white;
            border: 1px solid #dadce0;
            border-radius: 8px;
        }
        QFrame#frame:hover {
            border: 1px solid #1a73e8; /* Синя рамка при наведенні мишкою */
        }
        QLabel {
            border: none; /* Щоб лейбли не успадкували рамку від фрейму */
        }
    )");

    // Додаємо підказку на кнопку
    ui->toolButton->setToolTip("Копіювати інформацію про цю касу");
}

void PosCardWidget::createConnections()
{
    // Підключаємо клік по кнопці до нашого слота
    connect(ui->toolButton, &QToolButton::clicked, this, &PosCardWidget::onCopyClicked);
}

void PosCardWidget::setData(const QJsonObject &json, const QString &clientName, const QString &terminalId)
{
    // Зберігаємо дані у класі
    m_posData = json;
    m_clientName = clientName;
    m_terminalId = terminalId;

    // Витягуємо дані з JSON безпечно
    int posId = json["pos_id"].toInt();
    QString manufacturer = json["manufacturer"].toString();
    QString model = json["model"].toString();
    QString zn = json["factory_number"].toString();
    QString fn = json["tax_number"].toString();
    QString softVer = json["version"].toString();
    QString mukVer = json["muk_version"].toString();
    QString regDate = json["reg_date"].toString();

    // Заповнюємо UI

    ui->labelNameModel->setText(QString("<img src=':/res/Images/RRO_icon.png' width='16' height='16' align='middle'> Каса №%1 %2-%3").arg(posId).arg(manufacturer, model));
    ui->labelZnFn->setText(QString("⚙️ ЗН: %1    ФН: %2").arg(zn, fn));
    ui->labelSoftMuk->setText(QString("💻 ПО: %1 | МУК: %2").arg(softVer, mukVer));

    if (!regDate.isEmpty()) {
        ui->labelRegData->setText(QString("📅 Реєстрація: %1").arg(regDate));
    } else {
        ui->labelRegData->setText("📅 Реєстрація: Немає даних");
    }
}

void PosCardWidget::onCopyClicked()
{
    // Формуємо текст точно за вашим шаблоном
    int posId = m_posData["pos_id"].toInt();
    QString manufacturer = m_posData["manufacturer"].toString();
    QString model = m_posData["model"].toString();
    QString zn = m_posData["factory_number"].toString();
    QString fn = m_posData["tax_number"].toString();
    QString softVer = m_posData["version"].toString();
    QString mukVer = m_posData["muk_version"].toString();
    QString regDate = m_posData["reg_date"].toString();

    QStringList lines;

    // Шапка АЗС
    lines << QString("🏢 %1, АЗС %2").arg(m_clientName, m_terminalId);

    // Дані каси
    lines << QString("🧾 Каса №%1 %2-%3").arg(posId).arg(manufacturer, model);
    lines << QString("   ЗН: %1").arg(zn);
    lines << QString("   ФН: %1").arg(fn);
    lines << QString("   💻 ПО: %1 | МУК: %2").arg(softVer, mukVer);

    if (!regDate.isEmpty()) {
        lines << QString("   📅 Реєстрація: %1").arg(regDate);
    }

    // Відправляємо в буфер обміну
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(lines.join("\n"));
}
