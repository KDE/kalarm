/*
 *  akonadimodel.h  -  KAlarm calendar file access using Akonadi
 *  Program:  kalarm
 *  Copyright © 2010-2019 David Jarvie <djarvie@kde.org>
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

#ifndef AKONADIMODEL_H
#define AKONADIMODEL_H

#include "calendardatamodel.h"
#include "eventid.h"

#include <kalarmcal/kaevent.h>
#include <kalarmcal/collectionattribute.h>

#include <AkonadiCore/entitytreemodel.h>
#include <AkonadiCore/servermanager.h>

#include <QColor>
#include <QHash>
#include <QQueue>

namespace Akonadi
{
class ChangeRecorder;
}

class QPixmap;
class KJob;

using namespace KAlarmCal;


class AkonadiModel : public Akonadi::EntityTreeModel, public CalendarDataModel
{
        Q_OBJECT
    public:
        enum Change { Added, Deleted, Invalidated, Enabled, ReadOnly, AlarmTypes, WrongType, Location, Colour };

        /** Struct containing a KAEvent and its parent Collection. */
        struct Event
        {
            Event(const KAEvent& e, const Akonadi::Collection& c) : event(e), collection(c) {}
            EventId eventId() const       { return EventId(collection.id(), event.id()); }
            bool    isConsistent() const  { return event.collectionId() == collection.id(); }
            KAEvent             event;
            Akonadi::Collection collection;
        };
        typedef QList<Event> EventList;

        static AkonadiModel* instance();

        ~AkonadiModel() override;

        /** Return the display name for a collection. */
        QString displayName(Akonadi::Collection&) const;

        /** Return the storage type (file/directory/URL etc.) for a collection. */
        QString storageType(const Akonadi::Collection&) const;

        /** Get the foreground color for a collection, based on specified mime types. */
        static QColor foregroundColor(const Akonadi::Collection&, const QStringList& mimeTypes);

        /** Set the background color for a collection and its alarms. */
        void setBackgroundColor(Akonadi::Collection&, const QColor&);

        /** Get the background color for a collection and its alarms. */
        QColor backgroundColor(Akonadi::Collection&) const;

        /** Get the tooltip for a collection. The collection's enabled status is
         *  evaluated for specified alarm types. */
        QString tooltip(const Akonadi::Collection&, CalEvent::Types) const;

        /** Return the read-only status tooltip for a collection.
          * A null string is returned if the collection is fully writable. */
        static QString readOnlyTooltip(const Akonadi::Collection&);

        /** To be called when the command error status of an alarm has changed,
         *  to set in the Akonadi database and update the visual command error indications.
         */
        void updateCommandError(const KAEvent&);

        QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const override;
        bool setData(const QModelIndex&, const QVariant& value, int role) override;

        /** Refresh the specified collection instance with up to date data. */
        bool refresh(Akonadi::Collection&) const;
        /** Refresh the specified item instance with up to date data. */
        bool refresh(Akonadi::Item&) const;

        QModelIndex         collectionIndex(Akonadi::Collection::Id id) const
                                        { return collectionIndex(Akonadi::Collection(id)); }
        QModelIndex         collectionIndex(const Akonadi::Collection&) const;
        Akonadi::Collection collectionById(Akonadi::Collection::Id) const;
        Akonadi::Collection collectionForItem(Akonadi::Item::Id) const;
        Akonadi::Collection collection(const KAEvent& e) const   { return collectionForItem(e.itemId()); }

        /** Remove a collection from Akonadi. The calendar file is not removed.
         *  @return true if a removal job has been scheduled.
         */
        bool removeCollection(const Akonadi::Collection&);

        /** Reload a collection's data from Akonadi storage (not from the backend). */
        bool reloadCollection(const Akonadi::Collection&);

        /** Reload all collections' data from Akonadi storage (not from the backend). */
        void reload();

        /** Return whether calendar migration/creation at initialisation has completed. */
        bool isMigrationCompleted() const;

        bool isCollectionBeingDeleted(Akonadi::Collection::Id) const;

        QModelIndex         itemIndex(Akonadi::Item::Id id) const
                                        { return itemIndex(Akonadi::Item(id)); }
        QModelIndex         itemIndex(const Akonadi::Item&) const;
        Akonadi::Item       itemById(Akonadi::Item::Id) const;

        /** Return the alarm with the specified unique identifier.
         *  @return the event, or invalid event if no such event exists.
         */
        KAEvent event(const Akonadi::Item& item) const  { return event(item, QModelIndex(), nullptr); }
        KAEvent event(Akonadi::Item::Id) const;
        KAEvent event(const QModelIndex&) const;
        using QObject::event;   // prevent warning about hidden virtual method

        /** Return an event's model index, based on its itemId() value. */
        QModelIndex eventIndex(const KAEvent&);
        /** Search for an event's item ID. This method ignores any itemId() value
         *  contained in the KAEvent. The collectionId() is used if available.
         */
        Akonadi::Item::Id findItemId(const KAEvent&);

#if 0
        /** Return all events in a collection, optionally of a specified type. */
        KAEvent::List events(Akonadi::Collection&, CalEvent::Type = CalEvent::EMPTY) const;
#endif

        bool  addEvent(KAEvent&, Akonadi::Collection&);
        bool  addEvents(const KAEvent::List&, Akonadi::Collection&);
        bool  updateEvent(KAEvent& event);
        bool  deleteEvent(const KAEvent& event);

        /** Check whether a collection is stored in the current KAlarm calendar format. */
        static bool isCompatible(const Akonadi::Collection&);

        /** Return whether a collection is fully writable, i.e. with
         *  create/delete/change rights and compatible with the current KAlarm
         *  calendar format.
         *
         *  @param collection The collection to be inspected. This will be updated by
         *                    calling refresh().
         *  @return 1 = fully writable,
         *          0 = writable except that backend calendar is in an old KAlarm format,
         *         -1 = read-only or incompatible format.
         */
        static int isWritable(Akonadi::Collection&);

        /** Return whether a collection is fully writable, i.e. with
         *  create/delete/change rights and compatible with the current KAlarm
         *  calendar format.
         *
         *  @param collection The collection to be inspected. This will be updated by
         *                    calling refresh().
         *  @param format  Updated to contain the backend calendar storage format.
         *                 If read-only, = KACalendar::Current;
         *                 if unknown format, = KACalendar::Incompatible;
         *                 otherwise = the backend calendar storage format.
         *  @return 1 = fully writable,
         *          0 = writable except that backend calendar is in an old KAlarm format,
         *         -1 = read-only (if @p compat == KACalendar::Current), or
         *              incompatible format otherwise.
         */
        static int isWritable(Akonadi::Collection& collection, KACalendar::Compat& format);

        static CalEvent::Types types(const Akonadi::Collection&);

    Q_SIGNALS:
        /** Signal emitted when a collection has been added to the model. */
        void collectionAdded(const Akonadi::Collection&);

        /** Signal emitted when a collection's enabled, read-only or alarm types
         *  status has changed.
         *  @param change    The type of status which has changed
         *  @param newValue  The new value of the status that has changed
         *  @param inserted  true if the reason for the change is that the collection
         *                   has been inserted into the model
         */
        void collectionStatusChanged(const Akonadi::Collection&, AkonadiModel::Change change, const QVariant& newValue, bool inserted);

        /** Signal emitted when events have been added to the model. */
        void eventsAdded(const AkonadiModel::EventList&);

        /** Signal emitted when events are about to be removed from the model. */
        void eventsToBeRemoved(const AkonadiModel::EventList&);

        /** Signal emitted when an event in the model has changed. */
        void eventChanged(const AkonadiModel::Event&);

        /** Signal emitted when Akonadi has completed a collection deletion.
         *  @param id      Akonadi ID for the collection
         */
        void collectionDeleted(Akonadi::Collection::Id id);

        /** Signal emitted when Akonadi has completed an item creation, update
         *  or deletion.
         *  @param id      Akonadi ID for the item
         *  @param status  true if successful, false if error
         */
        void itemDone(Akonadi::Item::Id id, bool status = true);

        /** Signal emitted when calendar migration/creation has completed. */
        void migrationCompleted();

        /** Signal emitted when the Akonadi server has stopped. */
        void serverStopped();

    protected:
        QVariant entityHeaderData(int section, Qt::Orientation, int role, HeaderGroup) const override;
        int entityColumnCount(HeaderGroup) const override;

    private Q_SLOTS:
        void checkResources(Akonadi::ServerManager::State);
        void slotMigrationCompleted();
        void slotCollectionChanged(const Akonadi::Collection& c, const QSet<QByteArray>& attrNames);
        void slotCollectionRemoved(const Akonadi::Collection&);
        void slotCollectionBeingCreated(const QString& path, Akonadi::Collection::Id, bool finished);
        void slotUpdateTimeTo();
        void slotUpdateArchivedColour(const QColor&);
        void slotUpdateDisabledColour(const QColor&);
        void slotUpdateHolidays();
        void slotUpdateWorkingHours();
        void slotRowsInserted(const QModelIndex& parent, int start, int end);
        void slotRowsAboutToBeRemoved(const QModelIndex& parent, int start, int end);
        void slotMonitoredItemChanged(const Akonadi::Item&, const QSet<QByteArray>&);
        void slotEmitEventChanged();
        void modifyCollectionAttrJobDone(KJob*);
        void itemJobDone(KJob*);

    private:
        struct CalData   // data per collection
        {
            CalData()  : enabled(false) { }
            CalData(bool e, const QColor& c)  : colour(c), enabled(e) { }
            QColor colour;    // user selected color for the calendar
            bool   enabled;   // whether the collection is enabled
        };
        struct CollJobData   // collection data for jobs in progress
        {
            CollJobData() : id(-1) {}
            CollJobData(Akonadi::Collection::Id i, const QString& d) : id(i), displayName(d) {}
            Akonadi::Collection::Id id;
            QString                 displayName;
        };
        struct CollTypeData  // data for configuration dialog for collection creation job
        {
            CollTypeData() : parent(nullptr), alarmType(CalEvent::EMPTY) {}
            CollTypeData(CalEvent::Type t, QWidget* p) : parent(p), alarmType(t) {}
            QWidget*               parent;
            CalEvent::Type alarmType;
        };

        AkonadiModel(Akonadi::ChangeRecorder*, QObject* parent);
        void      initCalendarMigrator();
        KAEvent   event(const Akonadi::Item&, const QModelIndex&, Akonadi::Collection*) const;
        void      signalDataChanged(bool (*checkFunc)(const Akonadi::Item&), int startColumn, int endColumn, const QModelIndex& parent);
        void      setCollectionChanged(const Akonadi::Collection&, const QSet<QByteArray>&, bool rowInserted);
        void      queueItemModifyJob(const Akonadi::Item&);
        void      checkQueuedItemModifyJob(const Akonadi::Item&);
#if 0
        void     getChildEvents(const QModelIndex& parent, CalEvent::Type, KAEvent::List&) const;
#endif
        QColor    backgroundColor_p(const Akonadi::Collection&) const;
        EventList eventList(const QModelIndex& parent, int start, int end);

        static AkonadiModel*  mInstance;
        static int            mTimeHourPos;   // position of hour within time string, or -1 if leading zeroes included

        Akonadi::ChangeRecorder* mMonitor;
        QHash<Akonadi::Collection::Id, CalEvent::Types> mCollectionAlarmTypes;  // last content mime types of each collection
        QHash<Akonadi::Collection::Id, Akonadi::Collection::Rights> mCollectionRights;  // last writable status of each collection
        QHash<Akonadi::Collection::Id, CalEvent::Types> mCollectionEnabled;  // last enabled mime types of each collection
        QHash<Akonadi::Collection::Id, CollectionAttribute> mCollectionAttributes;  // current set value of CollectionAttribute of each collection
        QHash<Akonadi::Collection::Id, bool> mNewCollectionEnabled;  // enabled statuses of new collections
        QHash<KJob*, CollJobData> mPendingCollectionJobs;  // pending collection creation/deletion jobs, with collection ID & name
        QHash<KJob*, CollTypeData> mPendingColCreateJobs;  // default alarm type for pending collection creation jobs
        QHash<KJob*, Akonadi::Item::Id> mPendingItemJobs;  // pending item creation/deletion jobs, with event ID
        QHash<Akonadi::Item::Id, Akonadi::Item> mItemModifyJobQueue;  // pending item modification jobs, invalid item = queue empty but job active
        QList<QString>     mCollectionsBeingCreated;  // path names of new collections being created by migrator
        QList<Akonadi::Collection::Id> mCollectionIdsBeingCreated;  // ids of new collections being created by migrator
        QList<Akonadi::Item::Id> mItemsBeingCreated;  // new items not fully initialised yet
        QList<Akonadi::Collection::Id> mCollectionsDeleting;  // collections currently being removed
        QList<Akonadi::Collection::Id> mCollectionsDeleted;   // collections recently removed
        QQueue<Event>   mPendingEventChanges;   // changed events with changedEvent() signal pending
        bool            mResourcesChecked;      // whether resource existence has been checked yet
        bool            mMigrating;             // currently migrating calendars
};

#endif // AKONADIMODEL_H

// vim: et sw=4:
