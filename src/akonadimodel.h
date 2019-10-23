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
#include "resources/akonadiresource.h"

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
        enum Change { Enabled, ReadOnly, AlarmTypes };

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

        /** Get the tooltip for a resource. The resource's enabled status is
         *  evaluated for specified alarm types. */
        QString tooltip(const Resource&, CalEvent::Types) const;

        /** To be called when the command error status of an alarm has changed,
         *  to set in the Akonadi database and update the visual command error indications.
         */
        void updateCommandError(const KAEvent&);

        /** Refresh the specified collection instance with up to date data. */
        bool refresh(Akonadi::Collection&) const;

        Resource                 resource(Akonadi::Collection::Id) const;
        Resource                 resource(const KAEvent&) const;
        Resource                 resource(const QModelIndex&) const;
        QModelIndex              resourceIndex(const Resource&) const;
        QModelIndex              resourceIndex(Akonadi::Collection::Id) const;
        Resource                 resourceForEvent(const QString& eventId) const;
        Akonadi::Collection::Id  resourceIdForEvent(const QString& eventId) const;
        Akonadi::Collection*     collection(Akonadi::Collection::Id id) const;
        Akonadi::Collection*     collection(const Resource&) const;

        /** Remove a collection from Akonadi. The calendar file is not removed.
         *  @return true if a removal job has been scheduled.
         */
        bool removeCollection(Akonadi::Collection::Id);

        /** Reload a collection's data from Akonadi storage (not from the backend). */
        bool reloadResource(const Resource&);

        /** Reload all collections' data from Akonadi storage (not from the backend). */
        void reload();

        /** Return whether calendar migration/creation at initialisation has completed. */
        bool isMigrationCompleted() const;

        KAEvent event(const QString& eventId) const;
        KAEvent event(const QModelIndex&) const;
        using QObject::event;   // prevent warning about hidden virtual method

        /** Return an event's model index, based on its ID. */
        QModelIndex eventIndex(const KAEvent&) const;
        QModelIndex eventIndex(const QString& eventId) const;

#if 0
        /** Return all events in a collection, optionally of a specified type. */
        KAEvent::List events(Akonadi::Collection&, CalEvent::Type = CalEvent::EMPTY) const;
#endif

        bool  addEvent(KAEvent&, Resource&);
        bool  addEvents(const KAEvent::List&, Resource&);
        bool  updateEvent(KAEvent& event);
        bool  deleteEvent(const KAEvent& event);

        /** Called by a resource to notify an error message to display to the user.
         *  @param message  Must contain a '%1' to allow the resource's name to be substituted.
         */
        static void notifyResourceError(AkonadiResource*, const QString& message, const QString& details);

        /** Called by a resource to notify that a setting has changed. */
        static void notifySettingsChanged(AkonadiResource*, Change);

        QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const override;
        bool setData(const QModelIndex&, const QVariant& value, int role) override;

    Q_SIGNALS:
        /** Signal emitted when a collection has been added to the model. */
        void resourceAdded(Resource&);

        /** Signal emitted when a collection's enabled, read-only or alarm types
         *  status has changed.
         *  @param change    The type of status which has changed
         *  @param newValue  The new value of the status that has changed
         *  @param inserted  true if the reason for the change is that the collection
         *                   has been inserted into the model
         */
        void resourceStatusChanged(Resource&, AkonadiModel::Change change, const QVariant& newValue, bool inserted);

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

        /** Signal emitted when all collections have been populated. */
        void collectionsPopulated();

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
        void collectionFetchResult(KJob*);
        void slotCollectionChanged(const Akonadi::Collection& c, const QSet<QByteArray>& attrNames);
        void slotCollectionRemoved(const Akonadi::Collection&);
        void slotCollectionBeingCreated(const QString& path, Akonadi::Collection::Id, bool finished);
        void slotCollectionPopulated(Akonadi::Collection::Id);
        void slotUpdateTimeTo();
        void slotUpdateArchivedColour(const QColor&);
        void slotUpdateDisabledColour(const QColor&);
        void slotUpdateHolidays();
        void slotUpdateWorkingHours();
        void slotRowsInserted(const QModelIndex& parent, int start, int end);
        void slotRowsAboutToBeRemoved(const QModelIndex& parent, int start, int end);
        void slotMonitoredItemChanged(const Akonadi::Item&, const QSet<QByteArray>&);
        void slotEmitEventChanged();
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
        void          initCalendarMigrator();
        Resource&     updateResource(const Akonadi::Collection&) const;
        Akonadi::Collection collectionForItem(Akonadi::Item::Id) const;

        /** Return the alarm with the specified unique identifier.
         *  @return the event, or invalid event if no such event exists.
         */
        KAEvent       event(const Akonadi::Item& item) const  { return event(item, QModelIndex(), nullptr); }
        KAEvent       event(const Akonadi::Item&, const QModelIndex&, Akonadi::Collection*) const;
        QModelIndex   itemIndex(Akonadi::Item::Id id) const
                                        { return itemIndex(Akonadi::Item(id)); }
        QModelIndex   itemIndex(const Akonadi::Item&) const;
        Akonadi::Item itemById(Akonadi::Item::Id) const;
        void          signalDataChanged(bool (*checkFunc)(const Akonadi::Item&), int startColumn, int endColumn, const QModelIndex& parent);
        void          setCollectionChanged(Resource&, const Akonadi::Collection&, const QSet<QByteArray>&, bool rowInserted);
        void          handleEnabledChange(Resource&, CalEvent::Types newEnabled, bool rowInserted);
        void          queueItemModifyJob(const Akonadi::Item&);
        void          checkQueuedItemModifyJob(const Akonadi::Item&);
#if 0
        void     getChildEvents(const QModelIndex& parent, CalEvent::Type, KAEvent::List&) const;
#endif
        EventList     eventList(const QModelIndex& parent, int start, int end, bool inserted);

        static AkonadiModel*  mInstance;
        static int            mTimeHourPos;   // position of hour within time string, or -1 if leading zeroes included

        Akonadi::ChangeRecorder* mMonitor;
        QHash<Akonadi::Collection::Id, CalEvent::Types> mCollectionAlarmTypes;  // last content mime types of each collection
        QHash<Akonadi::Collection::Id, Akonadi::Collection::Rights> mCollectionRights;  // last writable status of each collection
        QHash<Akonadi::Collection::Id, CalEvent::Types> mCollectionEnabled;  // last enabled mime types of each collection
        QHash<KJob*, CollJobData> mPendingCollectionJobs;  // pending collection creation/deletion jobs, with collection ID & name
        QHash<KJob*, CollTypeData> mPendingColCreateJobs;  // default alarm type for pending collection creation jobs
        QHash<KJob*, Akonadi::Item::Id> mPendingItemJobs;  // pending item creation/deletion jobs, with event ID
        QHash<Akonadi::Item::Id, Akonadi::Item> mItemModifyJobQueue;  // pending item modification jobs, invalid item = queue empty but job active
        QList<QString>     mCollectionsBeingCreated;  // path names of new collections being created by migrator
        QList<Akonadi::Collection::Id> mCollectionIdsBeingCreated;  // ids of new collections being created by migrator
        struct EventIds
        {
            Akonadi::Collection::Id  collectionId{-1};
            Akonadi::Item::Id        itemId{-1};
            explicit EventIds(Akonadi::Collection::Id c = -1, Akonadi::Item::Id i = -1) : collectionId(c), itemId(i) {}
        };
        QHash<QString, EventIds> mEventIds;     // collection and item ID for each event ID
        QList<Akonadi::Item::Id> mItemsBeingCreated;  // new items not fully initialised yet
        mutable QHash<Akonadi::Collection::Id, Resource> mResources;
        QQueue<Event>   mPendingEventChanges;   // changed events with changedEvent() signal pending
        bool            mMigrationChecked;      // whether calendar migration has been checked at startup
        bool            mMigrating;             // currently migrating calendars
};

#endif // AKONADIMODEL_H

// vim: et sw=4:
