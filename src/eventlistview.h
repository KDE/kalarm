/*
 *  eventlistview.h  -  base class for widget showing list of alarms
 *  Program:  kalarm
 *  Copyright © 2007-2020 David Jarvie <djarvie@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
