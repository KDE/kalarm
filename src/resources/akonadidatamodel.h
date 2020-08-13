/*
 *  akonadidatamodel.h  -  KAlarm calendar file access using Akonadi
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2010-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
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
    void slotResourceMessage(ResourceType::MessageType, const QString& message, const QString& details);

Q_SIGNALS:
    /** Signal emitted when the Akonadi server has stopped. */
    void serverStopped();

protected:
    /** Terminate access to the data model, and tidy up. Not necessary for Akonadi. */
    void terminate() override  {}

    /** Reload all resources' data from storage.
     *  @note This reloads data from Akonadi storage, not from the backend storage.
     */
    void reload() override;

    /** Reload a resource's data from storage.
     *  @note This reloads data from Akonadi storage, not from the backend storage.
     */
    bool reload(Resource&) override;

    /** Check for, and remove, any duplicate resources, i.e. those which use
     *  the same calendar file/directory.
     */
    void removeDuplicateResources() override;

    /** Disable the widget if the database engine is not available, and display
     *  an error overlay.
     */
    void widgetNeedsDatabase(QWidget*) override;

    /** Create an AkonadiResourceCreator instance. */
    ResourceCreator* createResourceCreator(KAlarmCal::CalEvent::Type defaultType, QWidget* parent) override;

    /** Update a resource's backend calendar file to the current KAlarm format. */
    void updateCalendarToCurrentFormat(Resource&, bool ignoreKeepFormat, QObject* parent) override;

    ResourceListModel* createResourceListModel(QObject* parent) override;
    ResourceFilterCheckListModel* createResourceFilterCheckListModel(QObject* parent) override;
    AlarmListModel*    createAlarmListModel(QObject* parent) override;
    AlarmListModel*    allAlarmListModel() override;
    TemplateListModel* createTemplateListModel(QObject* parent) override;
    TemplateListModel* allTemplateListModel() override;

    /** Return the data storage backend type used by this model. */
    Preferences::Backend dataStorageBackend() const override   { return Preferences::Akonadi; }

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

    static bool   mInstanceIsOurs;        // mInstance is an AkonadiDataModel instance
    static int    mTimeHourPos;           // position of hour within time string, or -1 if leading zeroes included

    Akonadi::ChangeRecorder* mMonitor;
    QHash<KJob*, CollJobData> mPendingCollectionJobs;  // pending collection creation/deletion jobs, with collection ID & name
    QHash<KJob*, CollTypeData> mPendingColCreateJobs;  // default alarm type for pending collection creation jobs
    QList<QString>     mCollectionsBeingCreated;  // path names of new collections being created by migrator
    QList<Akonadi::Collection::Id> mCollectionIdsBeingCreated;  // ids of new collections being created by migrator
    struct EventIds
    {
        Akonadi::Collection::Id  collectionId {-1};
        Akonadi::Item::Id        itemId {-1};
        explicit EventIds(Akonadi::Collection::Id c = -1, Akonadi::Item::Id i = -1) : collectionId(c), itemId(i) {}
    };
    QHash<QString, EventIds> mEventIds;     // collection and item ID for each event ID
    mutable QHash<Akonadi::Collection::Id, Resource> mResources;
    QQueue<KAEvent> mPendingEventChanges;   // changed events with changedEvent() signal pending
};

#endif // AKONADIDATAMODEL_H

// vim: et sw=4:
