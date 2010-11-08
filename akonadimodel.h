/*
 *  akonadimodel.h  -  KAlarm calendar file access using Akonadi
 *  Program:  kalarm
 *  Copyright © 2010 by David Jarvie <djarvie@kde.org>
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

#include "kacalendar.h"
#include "kaevent.h"

#include <akonadi/entitytreemodel.h>

#include <QSize>
#include <QColor>
#include <QMap>
#include <QFont>
#include <QQueue>

namespace Akonadi {
    class AgentInstanceCreateJob;
}
class QPixmap;
class KJob;
class DateTime;


class AkonadiModel : public Akonadi::EntityTreeModel
{
        Q_OBJECT
    public:
        enum Change { Added, Deleted, Invalidated, Enabled, ReadOnly, WrongType, Location, Colour };
        enum {   // data columns
            // Item columns
            TimeColumn = 0, TimeToColumn, RepeatColumn, ColourColumn, TypeColumn, TextColumn,
            TemplateNameColumn,
            ColumnCount
        };
        enum {   // additional data roles
            // Collection and Item roles
            EnabledRole = UserRole,    // true for enabled alarm, false for disabled
            // Collection roles
            BaseColourRole,            // background colour ignoring collection colour
            AlarmTypeRole,             // OR of event types which collection contains
            IsStandardRole,            // OR of event types which collection is standard for
            // Item roles
            StatusRole,                // KAEvent::ACTIVE/ARCHIVED/TEMPLATE
            AlarmActionsRole,          // KAEvent::Actions
            AlarmActionRole,           // KAEvent::Action
            ValueRole,                 // numeric value
            SortRole,                  // the value to use for sorting
            CommandErrorRole           // last command execution error for alarm (per user)
        };

        /** Struct containing a KAEvent and its parent Collection. */
        struct Event
        {
            Event(const KAEvent& e, const Akonadi::Collection& c) : event(e), collection(c) {}
            KAEvent             event;
            Akonadi::Collection collection;
        };
        typedef QList<Event> EventList;

        static AkonadiModel* instance();

        /** Return the display name for a collection. */
        QString displayName(Akonadi::Collection&) const;
        /** Return the storage type (file/directory/URL etc.) for a collection. */
        QString storageType(const Akonadi::Collection&) const;
        /** Set the background color for a collection and its alarms. */
        void setBackgroundColor(Akonadi::Collection&, const QColor&);
        /** Get the background color for a collection and its alarms. */
        QColor backgroundColor(Akonadi::Collection&) const;

        /** To be called when the command error status of an alarm has changed,
         *  to set in the Akonadi database and update the visual command error indications.
         */
        void updateCommandError(const KAEvent&);

        virtual QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const;
        virtual bool setData(const QModelIndex&, const QVariant& value, int role);

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

        /** Create a new collection after prompting the user for its configuration.
         *  The signal collectionAdded() will be emitted once the collection is created.
         *  @return creation job which was started, or null if error.
         */
        Akonadi::AgentInstanceCreateJob* addCollection(KAlarm::CalEvent::Type, QWidget* parent = 0);

        /** Remove a collection from Akonadi. The calendar file is not removed.
         *  @return true if a removal job has been scheduled.
         */
        bool removeCollection(const Akonadi::Collection&);

        bool isCollectionBeingDeleted(Akonadi::Collection::Id) const;

        QModelIndex         itemIndex(Akonadi::Item::Id id) const
                                        { return itemIndex(Akonadi::Item(id)); }
        QModelIndex         itemIndex(const Akonadi::Item&) const;
        Akonadi::Item       itemById(Akonadi::Item::Id) const;

        /** Return the alarm with the specified unique identifier.
         *  @return the event, or invalid event if no such event exists.
         */
        KAEvent event(const Akonadi::Item&) const;
        KAEvent event(Akonadi::Item::Id) const;
        KAEvent event(const QModelIndex&) const;
        using QObject::event;   // prevent warning about hidden virtual method

        QModelIndex eventIndex(const KAEvent&);

#if 0
        /** Return all events in a collection, optionally of a specified type. */
        KAEvent::List events(Akonadi::Collection&, KAlarm::CalEvent::Type = KAlarm::CalEvent::EMPTY) const;
#endif

        bool  addEvent(KAEvent&, Akonadi::Collection&);
        bool  addEvents(const QList<KAEvent*>&, Akonadi::Collection&);
        bool  updateEvent(KAEvent& event);
        bool  updateEvent(Akonadi::Item::Id oldId, KAEvent& newEvent);
        bool  deleteEvent(const KAEvent& event);
        bool  deleteEvent(Akonadi::Item::Id itemId);

        static KAlarm::CalEvent::Types types(const Akonadi::Collection&);

        /** Check whether the alarm types in a local calendar correspond with a
         *  Collection's mime types.
         *  @return true if at least one alarm is the right type.
         */
        static bool checkAlarmTypes(const Akonadi::Collection&, KCalCore::Calendar::Ptr&);

        static QSize iconSize()  { return mIconSize; }

    signals:
        /** Signal emitted when a collection creation job has completed.
         *  Note that it may not yet have been added to the model.
         */
        void collectionAdded(Akonadi::AgentInstanceCreateJob*, bool success);

        /** Signal emitted when a collection's enabled or read-only status has changed. */
        void collectionStatusChanged(const Akonadi::Collection&, AkonadiModel::Change, bool value = false);

        /** Signal emitted when events have been added to the model. */
        void eventsAdded(const AkonadiModel::EventList&);

        /** Signal emitted when events are about to be removed from the model. */
        void eventsToBeRemoved(const AkonadiModel::EventList&);

        /** Signal emitted when an event in the model has changed. */
        void eventChanged(const AkonadiModel::Event&);

        /** Signal emitted when Akonadi has completed a collection modification.
         *  @param id      Akonadi ID for the collection
         *  @param status  true if successful, false if error
         */
        void collectionModified(Akonadi::Collection::Id, bool status = true);

        /** Signal emitted when Akonadi has completed a collection deletion.
         *  @param id      Akonadi ID for the collection
         *  @param status  true if successful, false if error
         */
        void collectionDeleted(Akonadi::Collection::Id, bool status = true);

        /** Signal emitted when Akonadi has completed an item creation, update
         *  or deletion.
         *  @param id      Akonadi ID for the item
         *  @param status  true if successful, false if error
         */
        void itemDone(Akonadi::Item::Id, bool status = true);

    protected:
        virtual QVariant entityHeaderData(int section, Qt::Orientation, int role, HeaderGroup) const;
        virtual int entityColumnCount(HeaderGroup) const;

    private slots:
        void slotCollectionChanged(const Akonadi::Collection&, const QSet<QByteArray>&);
        void slotCollectionRemoved(const Akonadi::Collection&);
        void slotUpdateTimeTo();
        void slotUpdateArchivedColour(const QColor&);
        void slotUpdateDisabledColour(const QColor&);
        void slotUpdateHolidays();
        void slotUpdateWorkingHours();
        void slotRowsInserted(const QModelIndex& parent, int start, int end);
        void slotRowsAboutToBeRemoved(const QModelIndex& parent, int start, int end);
        void slotMonitoredItemChanged(const Akonadi::Item&, const QSet<QByteArray>&);
        void slotEmitEventChanged();
        void addCollectionJobDone(KJob*);
        void modifyCollectionJobDone(KJob*);
        void deleteCollectionJobDone(KJob*);
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

        AkonadiModel(Akonadi::ChangeRecorder*, QObject* parent);
        QString   alarmTimeText(const DateTime&) const;
        QString   timeToAlarmText(const DateTime&) const;
        void      signalDataChanged(bool (*checkFunc)(const Akonadi::Item&), int startColumn, int endColumn, const QModelIndex& parent);
        void      queueItemModifyJob(const Akonadi::Item&);
        void      checkQueuedItemModifyJob(const Akonadi::Item&);
#if 0
        void     getChildEvents(const QModelIndex& parent, KAlarm::CalEvent::Type, KAEvent::List&) const;
#endif
        QString   displayName_p(const Akonadi::Collection&) const;
        QColor    backgroundColor_p(const Akonadi::Collection&) const;
        QString   repeatText(const KAEvent&) const;
        QString   repeatOrder(const KAEvent&) const;
        QPixmap*  eventIcon(const KAEvent&) const;
        QString   whatsThisText(int column) const;
        EventList eventList(const QModelIndex& parent, int start, int end);
        bool      setItemPayload(Akonadi::Item&, KAEvent&, const Akonadi::Collection&);

        static AkonadiModel*  mInstance;
        static QPixmap* mTextIcon;
        static QPixmap* mFileIcon;
        static QPixmap* mCommandIcon;
        static QPixmap* mEmailIcon;
        static QPixmap* mAudioIcon;
        static QSize    mIconSize;
        static int      mTimeHourPos;   // position of hour within time string, or -1 if leading zeroes included
        QMap<Akonadi::Collection::Id, Akonadi::Collection::Rights> mCollectionRights;  // last writable status of each collection
        QMap<Akonadi::Collection::Id, bool> mCollectionEnabled;  // last enabled status of each collection
        QMap<KJob*, CollJobData> mPendingCollectionJobs;  // pending collection creation/deletion jobs, with collection ID & name
        QMap<KJob*, Akonadi::Item::Id> mPendingItemJobs;  // pending item creation/deletion jobs, with event ID
        QMap<Akonadi::Item::Id, Akonadi::Item> mItemModifyJobQueue;  // pending item modification jobs, invalid item = queue empty but job active
        QList<Akonadi::Item::Id> mItemsBeingCreated;  // new items not fully initialised yet
        QList<Akonadi::Collection::Id> mCollectionsDeleting;  // collections currently being removed
        QQueue<Event>   mPendingEventChanges;   // changed events with changedEvent() signal pending
        QFont           mFont;
};

#endif // AKONADIMODEL_H

// vim: et sw=4:
