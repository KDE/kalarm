/*
 *  eventlistmodel.h  -  model class for lists of alarms or templates
 *  Program:  kalarm
 *  Copyright © 2007-2012 by David Jarvie <djarvie@kde.org>
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

#ifndef EVENTLISTMODEL_H
#define EVENTLISTMODEL_H

#include "kalarm.h"

#include "alarmresources.h"

#include <kalarmcal/kacalendar.h>
#include <kalarmcal/kaevent.h>

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QSize>

class QPixmap;
namespace KCal { class Event; }

using namespace KAlarmCal;


class EventListModel : public QAbstractTableModel
{
        Q_OBJECT
    public:
        enum {   // data columns
            TimeColumn, TimeToColumn, RepeatColumn, ColourColumn, TypeColumn, TextColumn,
            TemplateNameColumn,
            ColumnCount
        };
        enum {   // additional roles
            StatusRole = Qt::UserRole,  // return ACTIVE/ARCHIVED
            ValueRole,                  // return numeric value
            SortRole,                   // return the value to use for sorting
            EnabledRole                 // return true for enabled alarm, false for disabled
        };

        static EventListModel* alarms();
        static EventListModel* templates();
        ~EventListModel();
        virtual int           rowCount(const QModelIndex& parent = QModelIndex()) const;
        virtual int           columnCount(const QModelIndex& parent = QModelIndex()) const;
        virtual QModelIndex   index(int row, int column = 0, const QModelIndex& parent = QModelIndex()) const;
        virtual QVariant      data(const QModelIndex&, int role = Qt::DisplayRole) const;
        virtual bool          setData(const QModelIndex&, const QVariant& value, int role = Qt::EditRole);
        virtual QVariant      headerData(int section, Qt::Orientation, int role = Qt::DisplayRole) const;
        virtual Qt::ItemFlags flags(const QModelIndex&) const;
        static int            iconWidth()       { return mIconSize.width(); }
        QModelIndex           eventIndex(const KAEvent*) const;
        QModelIndex           eventIndex(const QString& eventId) const;
        void                  addEvent(KAEvent*);
        void                  addEvents(const KAEvent::List&);
        bool                  updateEvent(KAEvent* event)           { return updateEvent(mEvents.indexOf(event)); }
        bool                  updateEvent(const QString& eventId)   { return updateEvent(findEvent(eventId)); }
        bool                  updateEvent(const QString& oldId, KAEvent* newEvent);
        void                  removeEvent(const KAEvent* event)     { removeEvent(mEvents.indexOf(const_cast<KAEvent*>(event))); }
        void                  removeEvent(const QString& eventId)   { removeEvent(findEvent(eventId)); }
        void                  removeResource(AlarmResource*);
        KAEvent*              event(int row) const;
        static KAEvent*       event(const QModelIndex&);
        void                  updateCommandError(const QString& eventId);
        bool                  haveEvents() const                   { return mHaveEvents; }
        static void           resourceStatusChanged(AlarmResource*, AlarmResources::Change);

    public slots:
        void                  reload();

    signals:
        void                  haveEventsStatus(bool have);

    private slots:
        void     slotUpdateTimeTo();
        void     slotUpdateArchivedColour(const QColor&);
        void     slotUpdateDisabledColour(const QColor&);
        void     slotUpdateHolidays();
        void     slotUpdateWorkingHours();
        void     slotResourceLoaded(AlarmResource*, bool active);
        void     slotResourceStatusChanged(AlarmResource*, AlarmResources::Change);

    private:
        explicit EventListModel(CalEvent::Types, QObject* parent = 0);
        bool     updateEvent(int row);
        void     removeEvent(int row);
        int      findEvent(const QString& eventId) const;
        void     updateHaveEvents(bool have)        { mHaveEvents = have;  emit haveEventsStatus(have); }
        QString  repeatText(const KAEvent*) const;
        QString  repeatOrder(const KAEvent*) const;
        QPixmap* eventIcon(const KAEvent*) const;
        QString  whatsThisText(int column) const;

        static EventListModel* mAlarmInstance;     // the instance containing all alarms
        static EventListModel* mTemplateInstance;  // the instance containing all templates
        static QPixmap* mTextIcon;
        static QPixmap* mFileIcon;
        static QPixmap* mCommandIcon;
        static QPixmap* mEmailIcon;
        static QPixmap* mAudioIcon;
        static QSize    mIconSize;      // maximum size of any icon
        static int      mTimeHourPos;   // position of hour within time string, or -1 if leading zeroes included

        KAEvent::List   mEvents;
        CalEvent::Types mStatus;    // types of events contained in this model
        bool            mHaveEvents;// there are events in this model

        using QObject::event;   // prevent "hidden" warning
};


class EventListFilterModel : public QSortFilterProxyModel
{
        Q_OBJECT
    public:
        explicit EventListFilterModel(EventListModel* baseModel, QObject* parent = 0);
        KAEvent* event(int row) const;
        KAEvent* event(const QModelIndex&) const;
        QModelIndex eventIndex(const KAEvent*) const;
        QModelIndex eventIndex(const QString& eventId) const;
    
    private slots:
        void     slotDataChanged(const QModelIndex&, const QModelIndex&);

    private:
        using QObject::event;   // prevent "hidden" warning
};

#endif // EVENTLISTMODEL_H

// vim: et sw=4:
