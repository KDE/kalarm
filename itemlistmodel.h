/*
 *  itemlistmodel.h  -  Akonadi item models
 *  Program:  kalarm
 *  Copyright © 2010,2011 by David Jarvie <djarvie@kde.org>
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

#ifndef ITEMLISTMODEL_H
#define ITEMLISTMODEL_H

#include "akonadimodel.h"

#include <kalarmcal/kacalendar.h>
#include <kalarmcal/kaevent.h>

#include <AkonadiCore/entitymimetypefiltermodel.h>

using namespace KAlarmCal;

/*=============================================================================
= Class: ItemListModel
= Filter proxy model containing all items (alarms/templates) of specified mime
= types in enabled collections.
=============================================================================*/
class ItemListModel : public Akonadi::EntityMimeTypeFilterModel
{
        Q_OBJECT
    public:
        /** Constructor.
         *  @param allowed the alarm types (active/archived/template) included in this model
         */
        explicit ItemListModel(CalEvent::Types allowed, QObject* parent = 0);

        CalEvent::Types includedTypes() const  { return mAllowedTypes; }
        KAEvent      event(int row) const;
        KAEvent      event(const QModelIndex&) const;
        using QObject::event;   // prevent warning about hidden virtual method
        QModelIndex  eventIndex(Akonadi::Item::Id) const;

        /** Determine whether the model contains any items. */
        bool         haveEvents() const;

        virtual int  columnCount(const QModelIndex& parent = QModelIndex()) const;
        virtual Qt::ItemFlags flags(const QModelIndex&) const;

        static int   iconWidth()  { return AkonadiModel::iconSize().width(); }

    signals:
        /** Signal emitted when either the first item is added to the model,
         *  or when the last item is deleted from the model.
         */
        void         haveEventsStatus(bool have);

    protected:
        virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;

    private slots:
        void slotRowsInserted();
        void slotRowsRemoved();
        void collectionStatusChanged(const Akonadi::Collection& collection, AkonadiModel::Change change, const QVariant&, bool inserted);

    private:
        CalEvent::Types mAllowedTypes; // types of events allowed in this model
        bool            mHaveEvents;   // there are events in this model
};


/*=============================================================================
= Class: AlarmListModel
= Filter proxy model containing all alarms of specified mime types in enabled
= collections.
Equivalent to AlarmListFilterModel
=============================================================================*/
class AlarmListModel : public ItemListModel
{
        Q_OBJECT
    public:
        enum {   // data columns
            TimeColumn = 0, TimeToColumn, RepeatColumn, ColourColumn, TypeColumn, TextColumn,
            ColumnCount
        };

        explicit AlarmListModel(QObject* parent = 0);
        ~AlarmListModel();

        /** Return the model containing all active and archived alarms. */
        static AlarmListModel* all();

        /** Set a filter to restrict the event types to a subset of those
         *  specified in the constructor.
         *  @param types the event types to be included in the model
         */
        void setEventTypeFilter(CalEvent::Types types);

        /** Return the filter set by setEventTypeFilter().
         *  @return all event types included in the model
         */
        CalEvent::Types eventTypeFilter() const   { return mFilterTypes; }

        virtual int  columnCount(const QModelIndex& = QModelIndex()) const  { return ColumnCount; }
        virtual QVariant headerData(int section, Qt::Orientation, int role = Qt::DisplayRole) const;

    protected:
        virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;
        virtual bool filterAcceptsColumn(int sourceCol, const QModelIndex& sourceParent) const;

    private:
        static AlarmListModel* mAllInstance;

        CalEvent::Types mFilterTypes;  // types of events contained in this model
};


/*=============================================================================
= Class: TemplateListModel
= Filter proxy model containing all alarm templates for specified alarm types
= in enabled collections.
Equivalent to TemplateListFilterModel
=============================================================================*/
class TemplateListModel : public ItemListModel
{
        Q_OBJECT
    public:
        enum {   // data columns
            TypeColumn, TemplateNameColumn,
            ColumnCount
        };

        explicit TemplateListModel(QObject* parent = 0);
        ~TemplateListModel();

        /** Return the model containing all alarm templates. */
        static TemplateListModel* all();

        /** Set which alarm action types should be included in the model. */
        void setAlarmActionFilter(KAEvent::Actions);

        /** Return which alarm action types are included in the model. */
        KAEvent::Actions alarmActionFilter() const  { return mActionsFilter; }

        /** Set which alarm types should be shown as disabled in the model. */
        void setAlarmActionsEnabled(KAEvent::Actions);

        /** Set which alarm types should be shown as disabled in the model. */
        KAEvent::Actions setAlarmActionsEnabled() const  { return mActionsEnabled; }

        virtual int  columnCount(const QModelIndex& = QModelIndex()) const  { return ColumnCount; }
        virtual QVariant headerData(int section, Qt::Orientation, int role = Qt::DisplayRole) const;
        virtual Qt::ItemFlags flags(const QModelIndex&) const;

    protected:
        virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;
        virtual bool filterAcceptsColumn(int sourceCol, const QModelIndex& sourceParent) const;

    private:
        static TemplateListModel* mAllInstance;

        KAEvent::Actions mActionsEnabled;  // disable types not in this mask
        KAEvent::Actions mActionsFilter;   // hide types not in this mask
};

#endif // ITEMLISTMODEL_H

// vim: et sw=4:
