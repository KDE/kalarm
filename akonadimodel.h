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

#include <kacalendar.h>
#include "kaevent.h"
#include <akonadi/entitytreemodel.h>
#include <akonadi/entitymimetypefiltermodel.h>
#include <akonadi/favoritecollectionsmodel.h>
#include <akonadi_next/kreparentingproxymodel.h>
#include <kcalcore/calendar.h>
#include <QListView>
#include <QItemDelegate>
#include <QColor>
#include <QMap>
#include <QFont>
#include <QFlags>
#include <QQueue>

namespace Akonadi {
    class AgentInstanceCreateJob;
}
class QPixmap;
class KJob;
class KAEvent;
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
        QString displayName(const Akonadi::Collection&) const;
        /** Return the storage type (file/directory/URL etc.) for a collection. */
        QString storageType(const Akonadi::Collection&) const;
        /** Set the background color for a collection and its alarms. */
        void setBackgroundColor(Akonadi::Collection&, const QColor&);
        /** Get the background color for a collection and its alarms. */
        QColor backgroundColor(const Akonadi::Collection&) const;

        /** To be called when the command error status of an alarm has changed,
         *  to set in the Akonadi database and update the visual command error indications.
         */
        void updateCommandError(const KAEvent&);

        virtual QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const;
        virtual bool setData(const QModelIndex&, const QVariant& value, int role);

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

        /** Return the alarm with the specified unique identifier.
         *  @return the event, or invalid event if no such event exists.
         */
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
#if 0
        void     getChildEvents(const QModelIndex& parent, KAlarm::CalEvent::Type, KAEvent::List&) const;
#endif
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
        QMap<KJob*, CollJobData> mPendingCollections;  // pending collection creation/deletion jobs, with collection ID & name
        QMap<KJob*, Akonadi::Item::Id> mPendingItems;  // pending item creation/deletion jobs, with event ID
        QQueue<Event>   mPendingEventChanges;   // changed events with changedEvent() signal pending
        QFont           mFont;
};


/*=============================================================================
= Class: CollectionListModel
= Proxy model converting the collection tree into a flat list.
= The model may be restricted to specified content mime types.
=============================================================================*/
#include <kdescendantsproxymodel.h>
class CollectionListModel : public KDescendantsProxyModel
{
        Q_OBJECT
    public:
        explicit CollectionListModel(QObject* parent = 0);
        void setEventTypeFilter(KAlarm::CalEvent::Type);
        void setFilterWritable(bool writable);
        Akonadi::Collection collection(int row) const;
        Akonadi::Collection collection(const QModelIndex&) const;
        virtual bool isDescendantOf(const QModelIndex& ancestor, const QModelIndex& descendant) const;
};


/*=============================================================================
= Class: CollectionCheckListModel
= Proxy model providing a checkable collection list.
=============================================================================*/
#include <akonadi_next/kcheckableproxymodel.h>
class CollectionCheckListModel : public Future::KCheckableProxyModel
{
        Q_OBJECT
    public:
        static CollectionCheckListModel* instance();
        Akonadi::Collection collection(int row) const;
        Akonadi::Collection collection(const QModelIndex&) const;
        virtual bool setData(const QModelIndex&, const QVariant& value, int role);

    private slots:
        void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
        void slotRowsInserted(const QModelIndex& parent, int start, int end);

    private:
        explicit CollectionCheckListModel(QObject* parent = 0);

        static CollectionCheckListModel* mInstance;
        QItemSelectionModel* mSelectionModel;
};


/*=============================================================================
= Class: CollectionFilterCheckListModel
= Proxy model providing a checkable collection list, filtered by mime type.
=============================================================================*/
class CollectionFilterCheckListModel : public QSortFilterProxyModel
{
        Q_OBJECT
    public:
        explicit CollectionFilterCheckListModel(QObject* parent = 0);
        void setEventTypeFilter(KAlarm::CalEvent::Type);
        Akonadi::Collection collection(int row) const;
        Akonadi::Collection collection(const QModelIndex&) const;

    protected:
        bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;

    private:
        QString mMimeType;     // collection content type contained in this model
};


/*=============================================================================
= Class: CollectionView
Equivalent to ResourceView
=============================================================================*/
class CollectionView : public QListView
{
    public:
        explicit CollectionView(CollectionFilterCheckListModel*, QWidget* parent = 0);
        CollectionFilterCheckListModel* collectionModel() const  { return static_cast<CollectionFilterCheckListModel*>(model()); }
        Akonadi::Collection  collection(int row) const;
        Akonadi::Collection  collection(const QModelIndex&) const;

    protected:
        virtual void setModel(QAbstractItemModel*);
        virtual void mouseReleaseEvent(QMouseEvent*);
        virtual bool viewportEvent(QEvent*);
};


/*=============================================================================
= Class: CollectionControlModel
= Proxy model to select which Collections will be enabled. Disabled Collections
= are not populated or monitored; their contents are ignored. The set of
= enabled Collections is stored in the config file's "Collections" group.
= Note that this model is not used directly for displaying - its purpose is to
= allow collections to be disabled, which will remove them from the other
= collection models.
= This model also controls which collections are standard for their type,
= ensuring that there is only one standard collection for any given type.
=============================================================================*/
class CollectionControlModel : public Akonadi::FavoriteCollectionsModel
{
        Q_OBJECT
    public:
        static CollectionControlModel* instance();

        /** Return whether a collection is enabled (and valid). */
        static bool isEnabled(const Akonadi::Collection&);

        /** Enable or disable a collection (if it is valid). */
        static void setEnabled(const Akonadi::Collection&, bool enabled);

        /** Return whether a collection is both enabled and fully writable,
         *  i.e. with create/delete/change rights and compatible with the current
         *  KAlarm calendar format.
         *  Optionally, the enabled status can be ignored.
         */
        static bool isWritable(const Akonadi::Collection&, bool ignoreEnabledStatus = false);

        /** Return the standard collection for a specified mime type.
         *  Reply = invalid collection if there is no standard collection.
         */
        static Akonadi::Collection getStandard(KAlarm::CalEvent::Type);

        /** Return whether a collection is the standard collection for a specified
         *  mime type. */
        static bool isStandard(Akonadi::Collection&, KAlarm::CalEvent::Type);

        /** Return the alarm type(s) for which a collection is the standard collection. */
        static KAlarm::CalEvent::Types standardTypes(const Akonadi::Collection&);

        /** Set or clear a collection as the standard collection for a specified
         *  mime type. This does not affect its status for other mime types.
         */
        static void setStandard(Akonadi::Collection&, KAlarm::CalEvent::Type, bool standard);

        /** Set which mime types a collection is the standard collection for.
         *  Its standard status is cleared for other mime types.
         */
        static void setStandard(Akonadi::Collection&, KAlarm::CalEvent::Types);

        /** Set whether the user should be prompted for the destination collection
         *  to add alarms to.
         *  @param ask true = prompt for which collection to add to;
         *             false = add to standard collection.
         */
        static void setAskDestinationPolicy(bool ask)  { mAskDestination = ask; }

        /** Find the collection to be used to store an event of a given type.
         *  This will be the standard collection for the type, but if this is not valid,
         *  the user will be prompted to select a collection.
         *  @param noPrompt  don't prompt the user even if the standard collection is not valid
         *  @param cancelled If non-null: set to true if the user cancelled
         *             the prompt dialogue; set to false if any other error.
         */
        static Akonadi::Collection destination(KAlarm::CalEvent::Type, QWidget* promptParent = 0, bool noPrompt = false, bool* cancelled = 0);

        /** Return the enabled collections which contain a specified mime type.
         *  If 'writable' is true, only writable collections are included.
         */
        static Akonadi::Collection::List enabledCollections(KAlarm::CalEvent::Type, bool writable);

        virtual QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const;

    private slots:
        void statusChanged(const Akonadi::Collection&, AkonadiModel::Change, bool value);

    private:
        explicit CollectionControlModel(QObject* parent = 0);
        void findEnabledCollections(const Akonadi::EntityMimeTypeFilterModel*, const QModelIndex& parent, Akonadi::Collection::List&) const;

        static CollectionControlModel* mInstance;
        static bool mAskDestination;
};


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
        explicit ItemListModel(KAlarm::CalEvent::Types allowed, QObject* parent = 0);

        KAlarm::CalEvent::Types includedTypes() const  { return mAllowedTypes; }
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
        void slotRowsToBeRemoved();

    private:
        KAlarm::CalEvent::Types mAllowedTypes; // types of events allowed in this model
        bool                    mHaveEvents;   // there are events in this model
};


/*=============================================================================
= Class: AlarmListModel
= Filter proxy model containing all alarms of specified mime types in enabled
= collections.
Equivalent to AlarmListFilterModel
=============================================================================*/
class AlarmListModel : public ItemListModel
{
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
        void setEventTypeFilter(KAlarm::CalEvent::Types types);

        /** Return the filter set by setEventTypeFilter().
         *  @return all event types included in the model
         */
        KAlarm::CalEvent::Types eventTypeFilter() const   { return mFilterTypes; }

        virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const;

    protected:
        virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;
        virtual bool filterAcceptsColumn(int sourceCol, const QModelIndex& sourceParent) const;

    private:
        static AlarmListModel* mAllInstance;

        KAlarm::CalEvent::Types mFilterTypes;  // types of events contained in this model
};


/*=============================================================================
= Class: TemplateListModel
= Filter proxy model containing all alarm templates for specified alarm types
= in enabled collections.
Equivalent to TemplateListFilterModel
=============================================================================*/
class TemplateListModel : public ItemListModel
{
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

        virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const;
        virtual QModelIndex mapToSource(const QModelIndex& proxyIndex) const;
        virtual Qt::ItemFlags flags(const QModelIndex&) const;

    protected:
        virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;
        virtual bool filterAcceptsColumn(int sourceCol, const QModelIndex& sourceParent) const;

    private:
        static TemplateListModel* mAllInstance;

        KAEvent::Actions mActionsEnabled;  // disable types not in this mask
        KAEvent::Actions mActionsFilter;   // hide types not in this mask
};

#endif // AKONADIMODEL_H

// vim: et sw=4:
