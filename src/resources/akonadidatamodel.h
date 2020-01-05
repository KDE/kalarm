/*
 *  akonadidatamodel.h  -  KAlarm calendar file access using Akonadi
 *  Program:  kalarm
 *  Copyright © 2010-2020 David Jarvie <djarvie@kde.org>
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

#ifndef AKONADIDATAMODEL_H
#define AKONADIDATAMODEL_H

#include "resources/resourcedatamodelbase.h"
#include "resources/akonadiresource.h"

#include <KAlarmCal/KAEvent>

#include <AkonadiCore/EntityTreeModel>
#include <AkonadiCore/ServerManager>

#include <QColor>
#include <QHash>
#include <QQueue>

namespace Akonadi
{
class ChangeRecorder;
}

class KJob;

using namespace KAlarmCal;


class AkonadiDataModel : public Akonadi::EntityTreeModel, public ResourceDataModelBase
{
    Q_OBJECT
public:
    enum Change { Enabled, ReadOnly, AlarmTypes };

    static AkonadiDataModel* instance();

    ~AkonadiDataModel() override;

    static Akonadi::ChangeRecorder* monitor();

    /** Refresh the specified collection instance with up to date data. */
    bool refresh(Akonadi::Collection&) const;

    /** Refresh the specified item instance with up to date data. */
    bool refresh(Akonadi::Item&) const;

    Resource             resource(Akonadi::Collection::Id) const;
    Resource             resource(const QModelIndex&) const;
    QModelIndex          resourceIndex(const Resource&) const;
    QModelIndex          resourceIndex(Akonadi::Collection::Id) const;

    Akonadi::Collection* collection(Akonadi::Collection::Id id) const;
    Akonadi::Collection* collection(const Resource&) const;

    /** Reload a collection's data from Akonadi storage (not from the backend). */
    bool reload(const Resource&);

    /** Reload all collections' data from Akonadi storage (not from the backend). */
    void reload();

    KAEvent event(const QString& eventId) const;
    KAEvent event(const QModelIndex&) const;
    using QObject::event;   // prevent warning about hidden virtual method

    /** Return an event's model index, based on its ID. */
    QModelIndex eventIndex(const KAEvent&) const;
    QModelIndex eventIndex(const QString& eventId) const;

    /** Return the up-to-date Item, given its ID.
     *  If not found, an invalid Item is returned.
     */
    Akonadi::Item itemById(Akonadi::Item::Id) const;

    /** Return the Item for a given event. */
    Akonadi::Item itemForEvent(const QString& eventId) const;

    QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const override;

    int headerDataEventRoleOffset() const override;

private Q_SLOTS:
    /** Called when a resource notifies a message to display to the user. */
    void slotResourceMessage(Resource&, ResourceType::MessageType, const QString& message, const QString& details);

Q_SIGNALS:
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
    void slotCollectionTreeFetched();
    void slotCollectionPopulated(Akonadi::Collection::Id);
    void slotUpdateTimeTo();
    void slotUpdateArchivedColour(const QColor&);
    void slotUpdateDisabledColour(const QColor&);
    void slotUpdateHolidays();
    void slotUpdateWorkingHours();
    void slotRowsInserted(const QModelIndex& parent, int start, int end);
    void slotRowsAboutToBeRemoved(const QModelIndex& parent, int start, int end);
    void slotMonitoredItemChanged(const Akonadi::Item&, const QSet<QByteArray>&);
    void slotEmitEventUpdated();

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

    AkonadiDataModel(Akonadi::ChangeRecorder*, QObject* parent);
    void          initResourceMigrator();
    Resource&     updateResource(const Akonadi::Collection&) const;

    /** Return the alarm for the specified Akonadi Item.
     *  The item's parentCollection() is set.
     *  @param res  Set the resource for the item's parent collection.
     *  @return the event, or invalid event if no such event exists.
     */
    KAEvent       event(Akonadi::Item&, const QModelIndex&, Resource& res) const;
    QModelIndex   itemIndex(const Akonadi::Item&) const;
    void          signalDataChanged(bool (*checkFunc)(const Akonadi::Item&), int startColumn, int endColumn, const QModelIndex& parent);
    void          setCollectionChanged(Resource&, const Akonadi::Collection&, bool checkCompat);
    QList<KAEvent> events(ResourceId) const;
    void          getChildEvents(const QModelIndex& parent, QList<KAEvent>&) const;

    static AkonadiDataModel*  mInstance;
    static int            mTimeHourPos;   // position of hour within time string, or -1 if leading zeroes included

    Akonadi::ChangeRecorder* mMonitor;
    QHash<KJob*, CollJobData> mPendingCollectionJobs;  // pending collection creation/deletion jobs, with collection ID & name
    QHash<KJob*, CollTypeData> mPendingColCreateJobs;  // default alarm type for pending collection creation jobs
    QList<QString>     mCollectionsBeingCreated;  // path names of new collections being created by migrator
    QList<Akonadi::Collection::Id> mCollectionIdsBeingCreated;  // ids of new collections being created by migrator
    struct EventIds
    {
        Akonadi::Collection::Id  collectionId{-1};
        Akonadi::Item::Id        itemId{-1};
        explicit EventIds(Akonadi::Collection::Id c = -1, Akonadi::Item::Id i = -1) : collectionId(c), itemId(i) {}
    };
    QHash<QString, EventIds> mEventIds;     // collection and item ID for each event ID
    mutable QHash<Akonadi::Collection::Id, Resource> mResources;
    QQueue<KAEvent> mPendingEventChanges;   // changed events with changedEvent() signal pending
};

#endif // AKONADIDATAMODEL_H

// vim: et sw=4:
