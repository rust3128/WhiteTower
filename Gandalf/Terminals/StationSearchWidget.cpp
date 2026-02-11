#include "StationSearchWidget.h"
#include "Oracle/ApiClient.h"
#include "Oracle/Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QApplication>
#include <QKeyEvent>
#include <QColor>
#include <QGraphicsDropShadowEffect>
#include <QAction>
#include <QToolButton> // <--- ВАЖЛИВО: Додано для кнопки питання

// --- MODEL ---
StationListModel::StationListModel(QObject *parent) : QAbstractListModel(parent) {}
void StationListModel::setStations(const QList<StationStruct> &stations) {
    beginResetModel(); m_data = stations; endResetModel();
}
int StationListModel::rowCount(const QModelIndex &) const { return m_data.size(); }
const StationStruct& StationListModel::getStation(int row) const { return m_data.at(row); }
QVariant StationListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_data.size()) return QVariant();
    if (role == Qt::UserRole) return QVariant::fromValue(m_data[index.row()]);
    return QVariant();
}

// --- DELEGATE ---
void StationDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QStyleOptionViewItem opt = option;
    opt.state &= ~QStyle::State_HasFocus;

    StationStruct st = index.data(Qt::UserRole).value<StationStruct>();
    QRect r = opt.rect;

    if (opt.state & QStyle::State_Selected || opt.state & QStyle::State_MouseOver)
        painter->fillRect(r, QColor("#F1F3F4"));
    else
        painter->fillRect(r, Qt::white);

    // Логіка кольорів та символів
    QColor circleColor;
    QString symbol;
    QColor symbolColor = Qt::white;

    if (st.isActive && st.isWork) {
        circleColor = QColor("#34A853"); symbol = "✔";
    } else if (st.isActive && !st.isWork) {
        circleColor = QColor("#FBBC05"); symbol = "!"; symbolColor = Qt::black;
    } else if (!st.isActive && !st.isWork) {
        circleColor = QColor("#BDBDBD"); symbol = "✕";
    } else {
        circleColor = QColor("#EA4335"); symbol = "?";
    }

    int circleSize = 18;
    int circleX = r.left() + 15;
    int circleY = r.top() + (r.height() - circleSize) / 2;
    QRect circleRect(circleX, circleY, circleSize, circleSize);

    painter->setBrush(circleColor);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(circleRect);

    painter->setPen(symbolColor);
    QFont symbolFont = opt.font;
    symbolFont.setPixelSize(11);
    symbolFont.setBold(true);
    painter->setFont(symbolFont);
    painter->drawText(circleRect, Qt::AlignCenter, symbol);

    int textLeft = circleX + circleSize + 15;
    QRect textR(textLeft, r.top(), r.width() - textLeft - 10, r.height());

    painter->setPen(QColor("#202124"));
    QFont fTitle = opt.font;
    fTitle.setPixelSize(14);
    painter->setFont(fTitle);
    painter->drawText(textR.adjusted(0, 5, 0, 0), Qt::AlignLeft | Qt::AlignTop, st.clientName);

    painter->setPen(QColor("#70757A"));
    QFont fAddr = opt.font;
    fAddr.setPixelSize(12);
    painter->setFont(fAddr);
    painter->drawText(textR.adjusted(0, 0, 0, -5), Qt::AlignLeft | Qt::AlignBottom, st.address);

    painter->restore();
}
QSize StationDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const {
    return QSize(200, 52);
}

// --- WIDGET ---

StationSearchWidget::StationSearchWidget(QWidget *parent) : QWidget(parent) {
    // 1. Налаштування прозорості та розмірів
    setAttribute(Qt::WA_TranslucentBackground);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    this->setMaximumWidth(650);

    // Головний лейаут
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(0);

    // 2. Створення КОНТЕЙНЕРА (QFrame)
    m_searchContainer = new QFrame(this);
    m_searchContainer->setObjectName("searchContainer");
    m_searchContainer->setFixedHeight(48);

    // Лейаут всередині пігулки
    auto containerLayout = new QHBoxLayout(m_searchContainer);
    containerLayout->setContentsMargins(15, 0, 10, 0); // Відступи: 15 зліва, 10 справа
    containerLayout->setSpacing(5); // Відстань між елементами

    // А. Іконка Пошуку (Зліва)
    m_iconLabel = new QLabel(m_searchContainer);
    QIcon searchIcon(":/res/Images/search.png");
    if (searchIcon.isNull()) searchIcon = QApplication::style()->standardIcon(QStyle::SP_FileDialogContentsView);
    m_iconLabel->setPixmap(searchIcon.pixmap(20, 20));
    m_iconLabel->setFixedSize(24, 24);

    // Б. Поле вводу (Центр)
    m_input = new QLineEdit(m_searchContainer);
    m_input->setFrame(false);
    m_input->setPlaceholderText("Введіть Terminal ID...");
    m_input->setClearButtonEnabled(true); // Хрестик від Qt
    m_input->installEventFilter(this);
    m_input->setValidator(new QIntValidator(0, 999999, this));

    // В. Кнопка "Легенда" (Справа, окрема кнопка!)
    QToolButton *helpBtn = new QToolButton(m_searchContainer);
    helpBtn->setIcon(QApplication::style()->standardIcon(QStyle::SP_MessageBoxQuestion));
    helpBtn->setCursor(Qt::PointingHandCursor);
    helpBtn->setStyleSheet(R"(
        QToolButton {
            border: none;
            background: transparent;
            border-radius: 10px;
        }
        QToolButton:hover {
            background: #F1F3F4;
        }
    )");

    QString legendHtml = R"(
        <h3 style='margin:0; padding:0;'>Легенда статусів:</h3>
        <table border='0' cellspacing='5'>
            <tr>
                <td style='color:#34A853; font-size:16px;'>●</td>
                <td><b>Активна, Працює</b><br><span style='color:gray'>Все ок</span></td>
            </tr>
            <tr>
                <td style='color:#FBBC05; font-size:16px;'>●</td>
                <td><b>Активна, НЕ працює</b><br><span style='color:gray'>Ремонт / Світло</span></td>
            </tr>
            <tr>
                <td style='color:#BDBDBD; font-size:16px;'>●</td>
                <td><b>НЕ Активна, НЕ працює</b><br><span style='color:gray'>Архів / Продана</span></td>
            </tr>
            <tr>
                <td style='color:#EA4335; font-size:16px;'>●</td>
                <td><b>Помилка даних</b><br><span style='color:gray'>Не активна, але "працює"</span></td>
            </tr>
        </table>
    )";
    helpBtn->setToolTip(legendHtml);

    // Збираємо пігулку
    containerLayout->addWidget(m_iconLabel);
    containerLayout->addWidget(m_input);
    containerLayout->addWidget(helpBtn); // Додаємо кнопку в кінець

    // Додаємо тінь
    auto shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(25);
    shadow->setColor(QColor(0, 0, 0, 50));
    shadow->setOffset(0, 4);
    m_searchContainer->setGraphicsEffect(shadow);

    // 3. Інші елементи
    m_statusBar = new QLabel(this);
    m_statusBar->setObjectName("statusBar");
    m_statusBar->setVisible(false);

    m_listView = new QListView(this);
    m_listView->setObjectName("resultList");
    m_model = new StationListModel(this);
    m_listView->setModel(m_model);
    m_listView->setItemDelegate(new StationDelegate(this));
    m_listView->setFrameShape(QFrame::NoFrame);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_listView->setFocusPolicy(Qt::NoFocus);
    m_listView->setVisible(false);
    m_listView->installEventFilter(this);

    // Збираємо головний лейаут
    mainLayout->addWidget(m_searchContainer);
    mainLayout->addWidget(m_statusBar);
    mainLayout->addWidget(m_listView);

    updateStyles(false);

    connect(m_input, &QLineEdit::textChanged, this, &StationSearchWidget::onInputChanged);
    connect(m_input, &QLineEdit::returnPressed, this, &StationSearchWidget::onReturnPressed);
    connect(m_listView, &QListView::clicked, this, &StationSearchWidget::onListItemClicked);
    connect(&ApiClient::instance(), &ApiClient::stationSearchFinished, this, &StationSearchWidget::onSearchResults);
}

// --- ІНШІ МЕТОДИ КЛАСУ ---

bool StationSearchWidget::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (watched == m_input) {
            if (keyEvent->key() == Qt::Key_Down) {
                if (m_listView->isVisible() && m_model->rowCount() > 0) {
                    m_listView->setFocus();
                    m_listView->setCurrentIndex(m_model->index(0, 0));
                    return true;
                }
            }
        } else if (watched == m_listView) {
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                if (m_listView->currentIndex().isValid()) {
                    onListItemClicked(m_listView->currentIndex());
                    return true;
                }
            } else if (keyEvent->key() == Qt::Key_Up) {
                if (m_listView->currentIndex().row() == 0) {
                    m_input->setFocus();
                    return true;
                }
            } else if (keyEvent->key() == Qt::Key_Escape) {
                m_listView->setVisible(false);
                m_input->setFocus();
                updateStyles(false);
                return true;
            }
        }
    }
    if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) {
        if (watched == m_input || watched == m_listView) {
            updateStyles(m_listView->isVisible());
        }
    }
    return QWidget::eventFilter(watched, event);
}

void StationSearchWidget::updateStyles(bool listVisible, bool hasError) {
    bool isActive = m_input->hasFocus() || m_listView->hasFocus();
    QString borderColor;
    if (hasError) borderColor = "#D93025";
    else if (isActive) borderColor = "#4285f4";
    else borderColor = "transparent";

    QString containerRadius = listVisible ? "24px 24px 0px 0px" : "24px";

    QString containerStyle = QString(R"(
        QFrame#searchContainer {
            background-color: white;
            border: 1px solid %1;
            border-radius: %2;
        }
        QFrame#searchContainer:hover {
            border: 1px solid %3;
        }
    )").arg(borderColor, containerRadius, hasError ? "#D93025" : "#dfe1e5");

    m_searchContainer->setStyleSheet(containerStyle);

    m_input->setStyleSheet(R"(
        QLineEdit {
            background: transparent;
            border: none;
            font-size: 16px;
            font-family: 'Segoe UI', sans-serif;
            color: #202124;
        }
    )");

    if (listVisible) {
        QString listBorderColor = hasError ? "#D93025" : "#dfe1e5";
        QString otherStyles = QString(R"(
            QLabel#statusBar {
                background-color: white;
                color: #9AA0A6;
                font-size: 11px;
                border-left: 1px solid %1;
                border-right: 1px solid %1;
                padding: 5px 20px;
            }
            QListView#resultList {
                border: 1px solid %1;
                border-top: none;
                border-bottom-left-radius: 24px;
                border-bottom-right-radius: 24px;
                background-color: white;
                outline: none;
                padding-bottom: 10px;
            }
            QScrollBar:vertical {
                border: none;
                background: white;
                width: 8px;
                margin: 0px 0px 10px 0px;
            }
            QScrollBar::handle:vertical {
                background: #DADCE0;
                min-height: 20px;
                border-radius: 4px;
            }
        )").arg(listBorderColor);
        this->setStyleSheet(otherStyles);
    }
}

void StationSearchWidget::onInputChanged(const QString &text) {
    updateStyles(false, false);
    if (text.isEmpty()) {
        m_listView->setVisible(false);
        m_statusBar->setVisible(false);
    }
}

void StationSearchWidget::onReturnPressed() {
    QString text = m_input->text().trimmed();
    if (text.isEmpty()) return;
    bool ok;
    int termId = text.toInt(&ok);
    if (ok) {
        logInfo() << "UI: Searching station ID:" << termId;
        ApiClient::instance().searchStation(termId);
    } else {
        updateStyles(false, true);
    }
}

void StationSearchWidget::onSearchResults(const QList<StationStruct>& stations) {
    m_input->setFocus();

    if (stations.isEmpty()) {
        updateStyles(false, true);
        m_listView->setVisible(false);
        m_statusBar->setVisible(false);
    } else if (stations.size() == 1) {
        m_listView->setVisible(false);
        m_statusBar->setVisible(false);
        updateStyles(false, false);
        emit objectSelected(stations.first().objectId);
    } else {
        // 1. Створюємо копію списку для сортування
        QList<StationStruct> sortedStations = stations;

        // 2. Сортуємо: Спочатку за статусом (як у легенді), потім за ID
        std::sort(sortedStations.begin(), sortedStations.end(), [](const StationStruct &a, const StationStruct &b) {
            // Визначаємо пріоритет (менше число = вище в списку)
            auto getRank = [](const StationStruct &s) {
                if (s.isActive && s.isWork) return 1;  // Зелений (Top)
                if (s.isActive && !s.isWork) return 2; // Жовтий
                if (!s.isActive && !s.isWork) return 3; // Сірий
                return 4;                              // Червоний (Error)
            };

            int rankA = getRank(a);
            int rankB = getRank(b);

            if (rankA != rankB) {
                return rankA < rankB; // Сортування за статусом
            }

            // Якщо статус однаковий, сортуємо за Terminal ID (від меншого до більшого)
            return a.terminalId < b.terminalId;
        });

        // 3. Передаємо вже відсортований список
        m_model->setStations(sortedStations);

        m_statusBar->setText(QString("ЗНАЙДЕНО РЕЗУЛЬТАТІВ: %1").arg(sortedStations.size()));
        m_statusBar->setVisible(true);

        int rowHeight = 52;
        int maxRows = 5;
        int listHeight = qMin(sortedStations.size() * rowHeight, rowHeight * maxRows);

        m_listView->setFixedHeight(listHeight);
        m_listView->setVisible(true);
        updateStyles(true, false);
    }
}

void StationSearchWidget::onListItemClicked(const QModelIndex &index) {
    StationStruct st = m_model->getStation(index.row());
    m_listView->setVisible(false);
    m_statusBar->setVisible(false);
    updateStyles(false, false);
    m_input->setText(QString::number(st.terminalId));
    emit objectSelected(st.objectId);
}

void StationSearchWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && m_listView->isVisible()) {
        m_listView->setVisible(false);
        m_statusBar->setVisible(false);
        updateStyles(false, false);
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}
