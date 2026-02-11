#ifndef STATIONSEARCHWIDGET_H
#define STATIONSEARCHWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QListView>
#include <QAbstractListModel>
#include <QStyledItemDelegate>
#include <QLabel>
#include <QFrame> // Додано
#include "StationStruct.h"

// --- MODEL ---
class StationListModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit StationListModel(QObject *parent = nullptr);
    void setStations(const QList<StationStruct> &stations);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    const StationStruct& getStation(int row) const;
private:
    QList<StationStruct> m_data;
};

// --- DELEGATE ---
class StationDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

// --- WIDGET ---
class StationSearchWidget : public QWidget {
    Q_OBJECT
public:
    explicit StationSearchWidget(QWidget *parent = nullptr);

signals:
    void objectSelected(int objectId);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override; // Для обробки фокусу

private slots:
    void onInputChanged(const QString &text);
    void onReturnPressed();
    void onSearchResults(const QList<StationStruct>& stations);
    void onListItemClicked(const QModelIndex &index);

private:
    // Нова структура: Контейнер -> (Іконка + Поле)
    QFrame *m_searchContainer;
    QLineEdit *m_input;
    QLabel *m_iconLabel; // Окрема мітка для іконки

    QLabel *m_statusBar;
    QListView *m_listView;
    StationListModel *m_model;

    void updateStyles(bool listVisible, bool hasError = false);
};

#endif // STATIONSEARCHWIDGET_H
