/*
 *  eventlistview.h  -  base class for widget showing list of alarms
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef EVENTLISTVIEW_H
#define EVENTLISTVIEW_H

#include <KAlarmCal/KAEvent>

#include <QTreeView>
#include <QItemDelegate>

class EventListModel;
class Find;

using namespace KAlarmCal;

class EventListView : public QTreeView
{
    Q_OBJECT
public:
    explicit EventListView(QWidget* parent = nullptr);
    void              setModel(QAbstractItemModel*) override;
    EventListModel*   itemModel() const;
    KAEvent           event(int row) const;
    KAEvent           event(const QModelIndex&) const;
    void              select(const QModelIndex&, bool scrollToIndex = false);
    void              clearSelection();
    QModelIndex       selectedIndex() const;
    KAEvent           selectedEvent() const;
    QVector<KAEvent>  selectedEvents() const;
    void              setEditOnSingleClick(bool e) { mEditOnSingleClick = e; }
    bool              editOnSingleClick() const    { return mEditOnSingleClick; }

public Q_SLOTS:
    virtual void      slotFind();
    virtual void      slotFindNext()       { findNext(true); }
    virtual void      slotFindPrev()       { findNext(false); }

Q_SIGNALS:
    void              contextMenuRequested(const QPoint& globalPos);
    void              findActive(bool);

protected:
    void              resizeEvent(QResizeEvent*) override;
    bool              viewportEvent(QEvent*) override;
    void              contextMenuEvent(QContextMenuEvent*) override;

protected Q_SLOTS:
    virtual void      initSections() = 0;
    void              sortChanged(int column, Qt::SortOrder);

private:
    void              findNext(bool forward);

    Find*             mFind {nullptr};
    bool              mEditOnSingleClick {false};

    using QObject::event;   // prevent "hidden" warning
};

class EventListDelegate : public QItemDelegate
{
        Q_OBJECT
    public:
        explicit EventListDelegate(EventListView* parent = nullptr) : QItemDelegate(parent) {}
        QWidget*     createEditor(QWidget*, const QStyleOptionViewItem&, const QModelIndex&) const override  { return nullptr; }
        bool         editorEvent(QEvent*, QAbstractItemModel*, const QStyleOptionViewItem&, const QModelIndex&) override;
        virtual void edit(KAEvent*, EventListView*) = 0;
};

#endif // EVENTLISTVIEW_H

// vim: et sw=4:
