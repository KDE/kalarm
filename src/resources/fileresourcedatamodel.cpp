/*
 *  fileresourcedatamodel.cpp  -  model containing file system resources and their events
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

#include "fileresourcedatamodel.h"

#include "fileresourcecalendarupdater.h"
#include "fileresourcecreator.h"
#include "fileresourcemigrator.h"
#include "eventmodel.h"
#include "resourcemodel.h"
#include "resources.h"

#include "preferences.h"
#include "lib/synchtimer.h"
#include "kalarm_debug.h"

// Represents a resource or event within the data model.
struct FileResourceDataModel::Node
{
private:
    KAEvent* eitem;  // if type Event, the KAEvent, which is owned by this instance
    Resource ritem;  // if type Resource, the resource
    Resource owner;  // resource containing this KAEvent, or null
public:
    Type type;

    Node(Resource& r) : ritem(r), type(Type::Resource) {}
    Node(KAEvent* e, Resource& r) : eitem(e), owner(r), type(Type::Event) {}
    ~Node()
    {
        if (type == Type::Event)
            delete eitem;
    }
    Resource resource() const { return (type == Type::Resource) ? ritem : Resource(); }
    KAEvent* event() const    { return (type == Type::Event) ? eitem : nullptr; }
    Resource parent() const   { return (type == Type::Event) ? owner : Resource(); }
};


bool FileResourceDataModel::mInstanceIsOurs = false;

/******************************************************************************
* Returns the unique instance, creating it first if necessary.
*/
FileResourceDataModel* FileResourceDataModel::instance(QObject* parent)
{
    if (!mInstance)
    {
        mInstance = new FileResourceDataModel(parent);
        mInstanceIsOurs = true;
    }
    return mInstanceIsOurs ? (FileResourceDataModel*)mInstance : nullptr;
}

/******************************************************************************
*/
FileResourceDataModel::FileResourceDataModel(QObject* parent)
    : QAbstractItemModel(parent)
    , ResourceDataModelBase()
    , mHaveEvents(false)
{
    qCDebug(KALARM_LOG) << "FileResourceDataModel::FileResourceDataModel";

    // Create the vector of resource nodes for the model root.
    mResourceNodes[Resource()];

    // Get a list of all resources, and their alarms, if they have already been
    // created before this, by a previous call to FileResourceConfigManager::createResources().
    const QList<ResourceId> resourceIds = FileResourceConfigManager::resourceIds();
    for (ResourceId id : resourceIds)
    {
        Resource resource = Resources::resource(id);
        if (!mResourceNodes.contains(resource))
            addResource(resource);
    }

    Resources* resources = Resources::instance();
    connect(resources, &Resources::resourceAdded,
                 this, &FileResourceDataModel::addResource);
    connect(resources, &Resources::resourcePopulated,
                 this, &FileResourceDataModel::slotResourceLoaded);
    connect(resources, &Resources::resourceToBeRemoved,
                 this, &FileResourceDataModel::removeResource);
    connect(resources, &Resources::eventsAdded,
                 this, &FileResourceDataModel::slotEventsAdded);
    connect(resources, &Resources::eventUpdated,
                 this, &FileResourceDataModel::slotEventUpdated);
    connect(resources, &Resources::eventsToBeRemoved,
                 this, &FileResourceDataModel::deleteEvents);

    connect(resources, &Resources::settingsChanged,
                 this, &FileResourceDataModel::slotResourceSettingsChanged);
    connect(resources, &Resources::resourceMessage,
                 this, &FileResourceDataModel::slotResourceMessage, Qt::QueuedConnection);

    FileResourceConfigManager::createResources(this);
    setCalendarsCreated();

    FileResourceMigrator* migrator = FileResourceMigrator::instance();
    if (!migrator)
        setMigrationComplete();
    else
    {
        connect(migrator, &QObject::destroyed, this, &FileResourceDataModel::slotMigrationCompleted);
        setMigrationInitiated();
        migrator->start();
    }

    MinuteTimer::connect(this, SLOT(slotUpdateTimeTo()));
    Preferences::connect(SIGNAL(archivedColourChanged(QColor)), this, SLOT(slotUpdateArchivedColour(QColor)));
    Preferences::connect(SIGNAL(disabledColourChanged(QColor)), this, SLOT(slotUpdateDisabledColour(QColor)));
    Preferences::connect(SIGNAL(holidaysChanged(KHolidays::HolidayRegion)), this, SLOT(slotUpdateHolidays()));
    Preferences::connect(SIGNAL(workTimeChanged(QTime,QTime,QBitArray)), this, SLOT(slotUpdateWorkingHours()));
}

/******************************************************************************
*/
FileResourceDataModel::~FileResourceDataModel()
{
    qCDebug(KALARM_LOG) << "FileResourceDataModel::~FileResourceDataModel";
    ResourceFilterCheckListModel::disable();   // prevent resources being disabled when they are removed
    while (!mResources.isEmpty())
        removeResource(mResources.first());
    if (mInstance == this)
    {
        mInstance = nullptr;
        mInstanceIsOurs = false;
    }
    delete Resources::instance();
}

/******************************************************************************
* Return whether a model index refers to a resource or an event.
*/
FileResourceDataModel::Type FileResourceDataModel::type(const QModelIndex& ix) const
{
    if (ix.isValid())
    {
        const Node* node = reinterpret_cast<Node*>(ix.internalPointer());
        if (node)
            return node->type;
    }
    return Type::Error;
}

/******************************************************************************
* Return the resource with the given ID.
*/
Resource FileResourceDataModel::resource(ResourceId id) const
{
    return Resource(Resources::resource(id));
}

/******************************************************************************
* Return the resource referred to by an index, or invalid resource if the index
* is not for a resource.
*/
Resource FileResourceDataModel::resource(const QModelIndex& ix) const
{
    if (ix.isValid())
    {
        const Node* node = reinterpret_cast<Node*>(ix.internalPointer());
        if (node)
        {
            Resource res = node->resource();
            if (!res.isNull())
                return res;
        }
    }
    return Resource();
}

/******************************************************************************
* Find the QModelIndex of a resource.
*/
QModelIndex FileResourceDataModel::resourceIndex(const Resource& resource) const
{
    if (resource.isValid())
    {
        int row = mResources.indexOf(const_cast<Resource&>(resource));
        if (row >= 0)
            return createIndex(row, 0, mResourceNodes.value(Resource()).at(row));
    }
    return QModelIndex();
}

/******************************************************************************
* Find the QModelIndex of a resource.
*/
QModelIndex FileResourceDataModel::resourceIndex(ResourceId id) const
{
    return resourceIndex(Resources::resource(id));
}

/******************************************************************************
* Return the event with the given ID.
*/
KAEvent FileResourceDataModel::event(const QString& eventId) const
{
    const Node* node = mEventNodes.value(eventId, nullptr);
    if (node)
    {
        KAEvent* event = node->event();
        if (event)
            return *event;
    }
    return KAEvent();
}

/******************************************************************************
* Return the event referred to by an index, or invalid event if the index is
* not for an event.
*/
KAEvent FileResourceDataModel::event(const QModelIndex& ix) const
{
    if (ix.isValid())
    {
        const Node* node = reinterpret_cast<Node*>(ix.internalPointer());
        if (node)
        {
            const KAEvent* event = node->event();
            if (event)
                return *event;
        }
    }
    return KAEvent();
}

/******************************************************************************
* Return the index to a specified event.
*/
QModelIndex FileResourceDataModel::eventIndex(const QString& eventId) const
{
    Node* node = mEventNodes.value(eventId, nullptr);
    if (node)
    {
        Resource resource = node->parent();
        if (resource.isValid())
        {
            const QVector<Node*> nodes = mResourceNodes.value(resource);
            int row = nodes.indexOf(node);
            if (row >= 0)
                return createIndex(row, 0, node);
        }
    }
    return QModelIndex();
}

/******************************************************************************
* Return the index to a specified event.
*/
QModelIndex FileResourceDataModel::eventIndex(const KAEvent& event) const
{
    return eventIndex(event.id());
}

/******************************************************************************
* Add an event to a specified resource.
* The event's 'updated' flag is cleared.
* Reply = true if item creation has been scheduled.
*/
bool FileResourceDataModel::addEvent(KAEvent& event, Resource& resource)
{
    qCDebug(KALARM_LOG) << "FileResourceDataModel::addEvent: ID:" << event.id();
    return resource.addEvent(event);
}

/******************************************************************************
* Recursive function to Q_EMIT the dataChanged() signal for all events in a
* specified column range.
*/
void FileResourceDataModel::signalDataChanged(bool (*checkFunc)(const KAEvent*), int startColumn, int endColumn, const QModelIndex& parent)
{
    int start = -1;
    int end   = -1;
    for (int row = 0, count = rowCount(parent);  row < count;  ++row)
    {
        KAEvent* event = nullptr;
        const QModelIndex ix = index(row, 0, parent);
        const Node* node = reinterpret_cast<Node*>(ix.internalPointer());
        if (node)
        {
            event = node->event();
            if (event)
            {
                if ((*checkFunc)(event))
                {
                    // For efficiency, emit a single signal for each group of
                    // consecutive events, rather than a separate signal for each event.
                    if (start < 0)
                        start = row;
                    end = row;
                    continue;
                }
            }
        }
        if (start >= 0)
            Q_EMIT dataChanged(index(start, startColumn, parent), index(end, endColumn, parent));
        start = -1;
        if (!event)
            signalDataChanged(checkFunc, startColumn, endColumn, ix);
    }

    if (start >= 0)
        Q_EMIT dataChanged(index(start, startColumn, parent), index(end, endColumn, parent));
}

void FileResourceDataModel::slotMigrationCompleted()
{
    qCDebug(KALARM_LOG) << "FileResourceDataModel: Migration completed";
    setMigrationComplete();
}

/******************************************************************************
* Signal every minute that the time-to-alarm values have changed.
*/
static bool checkEvent_isActive(const KAEvent* event)
{ return event->category() == CalEvent::ACTIVE; }

void FileResourceDataModel::slotUpdateTimeTo()
{
    signalDataChanged(&checkEvent_isActive, TimeToColumn, TimeToColumn, QModelIndex());
}

/******************************************************************************
* Called when the colour used to display archived alarms has changed.
*/
static bool checkEvent_isArchived(const KAEvent* event)
{ return event->category() == CalEvent::ARCHIVED; }

void FileResourceDataModel::slotUpdateArchivedColour(const QColor&)
{
    qCDebug(KALARM_LOG) << "FileResourceDataModel::slotUpdateArchivedColour";
    signalDataChanged(&checkEvent_isArchived, 0, ColumnCount - 1, QModelIndex());
}

/******************************************************************************
* Called when the colour used to display disabled alarms has changed.
*/
static bool checkEvent_isDisabled(const KAEvent* event)
{
    return !event->enabled();
}

void FileResourceDataModel::slotUpdateDisabledColour(const QColor&)
{
    qCDebug(KALARM_LOG) << "FileResourceDataModel::slotUpdateDisabledColour";
    signalDataChanged(&checkEvent_isDisabled, 0, ColumnCount - 1, QModelIndex());
}

/******************************************************************************
* Called when the definition of holidays has changed.
* Update the next trigger time for all alarms which are set to recur only on
* non-holidays.
*/
static bool checkEvent_excludesHolidays(const KAEvent* event)
{
    return event->holidaysExcluded();
}

void FileResourceDataModel::slotUpdateHolidays()
{
    qCDebug(KALARM_LOG) << "FileResourceDataModel::slotUpdateHolidays";
    Q_ASSERT(TimeToColumn == TimeColumn + 1);  // signal should be emitted only for TimeTo and Time columns
    signalDataChanged(&checkEvent_excludesHolidays, TimeColumn, TimeToColumn, QModelIndex());
}

/******************************************************************************
* Called when the definition of working hours has changed.
* Update the next trigger time for all alarms which are set to recur only
* during working hours.
*/
static bool checkEvent_workTimeOnly(const KAEvent* event)
{
    return event->workTimeOnly();
}

void FileResourceDataModel::slotUpdateWorkingHours()
{
    qCDebug(KALARM_LOG) << "FileResourceDataModel::slotUpdateWorkingHours";
    Q_ASSERT(TimeToColumn == TimeColumn + 1);  // signal should be emitted only for TimeTo and Time columns
    signalDataChanged(&checkEvent_workTimeOnly, TimeColumn, TimeToColumn, QModelIndex());
}

/******************************************************************************
* Called when loading of a resource is complete.
*/
void FileResourceDataModel::slotResourceLoaded(Resource& resource)
{
    qCDebug(KALARM_LOG) << "FileResourceDataModel::slotResourceLoaded:" << resource.displayName();
    addResource(resource);
}

/******************************************************************************
* Called when a resource setting has changed.
*/
void FileResourceDataModel::slotResourceSettingsChanged(Resource& res, ResourceType::Changes change)
{
    if (change & ResourceType::Enabled)
    {
        if (res.enabledTypes())
        {
            qCDebug(KALARM_LOG) << "FileResourceDataModel::slotResourceSettingsChanged: Enabled" << res.displayName();
            addResource(res);
        }
        else
        {
            qCDebug(KALARM_LOG) << "FileResourceDataModel::slotResourceSettingsChanged: Disabled" << res.displayName();
            removeResourceEvents(res);
        }
    }
    if (change & (ResourceType::Name | ResourceType::Standard | ResourceType::ReadOnly))
    {
        qCDebug(KALARM_LOG) << "FileResourceDataModel::slotResourceSettingsChanged:" << res.displayName();
        const QModelIndex resourceIx = resourceIndex(res);
        if (resourceIx.isValid())
            Q_EMIT dataChanged(resourceIx, resourceIx);
    }
    if (change & ResourceType::BackgroundColour)
    {
        qCDebug(KALARM_LOG) << "FileResourceDataModel::slotResourceSettingsChanged: Colour" << res.displayName();
        const QVector<Node*>& eventNodes = mResourceNodes.value(res);
        const int lastRow = eventNodes.count() - 1;
        if (lastRow >= 0)
            Q_EMIT dataChanged(createIndex(0, 0, eventNodes[0]), createIndex(lastRow, ColumnCount - 1, eventNodes[lastRow]));
    }

//    if (change & (ResourceType::AlarmTypes | ResourceType::KeepFormat | ResourceType::UpdateFormat))
//        qCDebug(KALARM_LOG) << "FileResourceDataModel::slotResourceSettingsChanged: UNHANDLED" << res.displayName();
}

/******************************************************************************
* Called when events have been added to a resource. Add events to the list.
*/
void FileResourceDataModel::slotEventsAdded(Resource& resource, const QList<KAEvent>& events)
{
    if (events.isEmpty())
        return;
    qCDebug(KALARM_LOG) << "FileResourceDataModel::slotEventsAdded:" << resource.displayId() << "Count:" << events.count();
    if (mResourceNodes.contains(resource))
    {
        // If events with the same ID already exist, remove them first.
        QList<KAEvent> eventsToAdd = events;
        {
            QList<KAEvent> eventsToDelete;
            for (int i = eventsToAdd.count();  --i >= 0;  )
            {
                const KAEvent& event = eventsToAdd.at(i);
                const Node* dnode = mEventNodes.value(event.id(), nullptr);
                if (dnode)
                {
                    if (dnode->parent() != resource)
                    {
                        qCWarning(KALARM_LOG) << "FileResourceDataModel::slotEventsAdded: Event ID already exists in another resource";
                        eventsToAdd.removeAt(i);
                    }
                    else
                        eventsToDelete << *dnode->event();
                }
            }
            if (!eventsToDelete.isEmpty())
                deleteEvents(resource, eventsToDelete);
        }

        if (!eventsToAdd.isEmpty())
        {
            QVector<Node*>& resourceEventNodes = mResourceNodes[resource];
            int row = resourceEventNodes.count();
            const QModelIndex resourceIx = resourceIndex(resource);
            beginInsertRows(resourceIx, row, row + eventsToAdd.count() - 1);
            for (const KAEvent& event : qAsConst(eventsToAdd))
            {
                KAEvent* ev = new KAEvent(event);
                ev->setResourceId(resource.id());
                Node* node = new Node(ev, resource);
                resourceEventNodes += node;
                mEventNodes[ev->id()] = node;
            }
            endInsertRows();
            if (!mHaveEvents)
                updateHaveEvents(true);
        }
    }
}

/******************************************************************************
* Update an event which already exists (and with the same UID) in the model.
*/
void FileResourceDataModel::slotEventUpdated(Resource& resource, const KAEvent& event)
{
    auto it = mEventNodes.find(event.id());
    if (it != mEventNodes.end())
    {
        Node* node = it.value();
        if (node  &&  node->parent() == resource)
        {
            KAEvent* oldEvent = node->event();
            if (oldEvent)
            {
                *oldEvent = event;

                const QVector<Node*> eventNodes = mResourceNodes.value(resource);
                int row = eventNodes.indexOf(node);
                if (row >= 0)
                {
                    const QModelIndex resourceIx = resourceIndex(resource);
                    Q_EMIT dataChanged(index(row, 0, resourceIx), index(row, ColumnCount - 1, resourceIx));
                }
            }
        }
    }
}

/******************************************************************************
* Delete events from their resource.
*/
bool FileResourceDataModel::deleteEvents(Resource& resource, const QList<KAEvent>& events)
{
    qCDebug(KALARM_LOG) << "FileResourceDataModel::deleteEvents:" << resource.displayName() << "Count:" << events.count();
    QModelIndex resourceIx = resourceIndex(resource);
    if (!resourceIx.isValid())
        return false;
    auto it = mResourceNodes.find(resource);
    if (it == mResourceNodes.end())
        return false;
    QVector<Node*>& eventNodes = it.value();

    // Find the row numbers of the events to delete.
    QVector<int> rowsToDelete;
    for (const KAEvent& event : events)
    {
        Node* node = mEventNodes.value(event.id(), nullptr);
        if (node  &&  node->parent() == resource)
        {
            int row = eventNodes.indexOf(node);
            if (row >= 0)
                rowsToDelete << row;
        }
    }

    // Delete the events in groups of consecutive rows (if any).
    std::sort(rowsToDelete.begin(), rowsToDelete.end());
    for (int i = 0, count = rowsToDelete.count();  i < count;  )
    {
        int row = rowsToDelete.at(i);
        int lastRow = row;
        while (++i < count  &&  rowsToDelete.at(i) == lastRow + 1)
            ++lastRow;

        beginRemoveRows(resourceIx, row, lastRow);
        do
        {
            Node* node = eventNodes.at(row);
            eventNodes.removeAt(row);
            mEventNodes.remove(node->event()->id());
            delete node;
        } while (++row <= lastRow);
        endRemoveRows();
    }

    if (mHaveEvents  &&  mEventNodes.isEmpty())
        updateHaveEvents(false);
    return true;
}

/******************************************************************************
* Add a resource and all its events into the model.
*/
void FileResourceDataModel::addResource(Resource& resource)
{
    // Get the events to add to the model
    const QList<KAEvent> events = resource.events();
    qCDebug(KALARM_LOG) << "FileResourceDataModel::addResource" << resource.displayName() << ", event count:" << events.count();

    const QModelIndex resourceIx = resourceIndex(resource);
    if (resourceIx.isValid())
    {
        // The resource already exists: remove its existing events from the model.
        bool noEvents = events.isEmpty();
        removeResourceEvents(resource, noEvents);
        if (noEvents)
            return;
        beginInsertRows(resourceIx, 0, events.count() - 1);
    }
    else
    {
        // Add the new resource to the model
        QVector<Node*>& resourceNodes = mResourceNodes[Resource()];
        int row = resourceNodes.count();
        beginInsertRows(QModelIndex(), row, row);
        mResources += resource;
        resourceNodes += new Node(resource);
        mResourceNodes.insert(resource, QVector<Node*>());
    }

    if (!events.isEmpty())
    {
        QVector<Node*>& resourceEventNodes = mResourceNodes[resource];
        for (const KAEvent& event : events)
        {
            Node* node = new Node(new KAEvent(event), resource);
            resourceEventNodes += node;
            mEventNodes[event.id()] = node;
        }
    }
    endInsertRows();
    if (!mHaveEvents  &&  !mEventNodes.isEmpty())
        updateHaveEvents(true);
    else if (mHaveEvents  &&  mEventNodes.isEmpty())
        updateHaveEvents(false);
}

/******************************************************************************
* Remove a resource and its events from the list.
* This has to be called before the resource is actually deleted or reloaded. If
* not, timer based updates can occur between the resource being deleted and
* slotResourceSettingsChanged(Deleted) being triggered, leading to crashes when
* data from the resource's events is fetched.
*/
void FileResourceDataModel::removeResource(Resource& resource)
{
    qCDebug(KALARM_LOG) << "FileResourceDataModel::removeResource" << resource.displayName();
    int row = mResources.indexOf(resource);
    if (row < 0)
        return;

    Resource r(resource);   // in case 'resource' is a reference to the copy in mResources
    int count = 0;
    beginRemoveRows(QModelIndex(), row, row);
    mResources.removeAt(row);
    QVector<Node*>& resourceNodes = mResourceNodes[Resource()];
    delete resourceNodes.at(row);
    resourceNodes.removeAt(row);
    auto it = mResourceNodes.find(r);
    if (it != mResourceNodes.end())
    {
        count = removeResourceEvents(it.value());
        mResourceNodes.erase(it);
    }
    endRemoveRows();

    if (count)
    {
        if (mHaveEvents  &&  mEventNodes.isEmpty())
            updateHaveEvents(false);
    }
}

/******************************************************************************
* Remove a resource's events from the list.
*/
void FileResourceDataModel::removeResourceEvents(Resource& resource, bool setHaveEvents)
{
    qCDebug(KALARM_LOG) << "FileResourceDataModel::removeResourceEvents" << resource.displayName();
    const QModelIndex resourceIx = resourceIndex(resource);
    if (resourceIx.isValid())
    {
        // The resource already exists: remove its existing events from the model.
        QVector<Node*>& eventNodes = mResourceNodes[resource];
        if (!eventNodes.isEmpty())
        {
            beginRemoveRows(resourceIx, 0, eventNodes.count() - 1);
            int count = removeResourceEvents(eventNodes);
            endRemoveRows();
            if (count  &&  setHaveEvents)
            {
                if (mHaveEvents  &&  mEventNodes.isEmpty())
                    updateHaveEvents(false);
            }
        }
    }
}

/******************************************************************************
* Remove a resource's events from the list.
* beginRemoveRows() must be called before this method, and endRemoveRows()
* afterwards. Then, removeConfigEvents() must be called with the return value
* from this method as a parameter.
* Return - number of events which have been removed.
*/
int FileResourceDataModel::removeResourceEvents(QVector<Node*>& eventNodes)
{
    qCDebug(KALARM_LOG) << "FileResourceDataModel::removeResourceEvents";
    int count = 0;
    for (Node* node : eventNodes)
    {
        KAEvent* event = node->event();
        if (event)
        {
            const QString eventId = event->id();
            mEventNodes.remove(eventId);
            ++count;
        }
        delete node;
    }
    eventNodes.clear();
    return count;
}

/******************************************************************************
* Terminate access to the data model, and tidy up.
*/
void FileResourceDataModel::terminate()
{
    delete mInstance;
}

/******************************************************************************
* Reload all resources' data from storage.
*/
void FileResourceDataModel::reload()
{
    for (int i = 0, iMax = mResources.count();  i < iMax;  ++i)
        mResources[i].reload();
}

/******************************************************************************
* Reload a resource's data from storage.
*/
bool FileResourceDataModel::reload(Resource& resource)
{
    if (!resource.isValid())
        return false;
    qCDebug(KALARM_LOG) << "FileResourceDataModel::reload:" << resource.displayId();
    return resource.reload();
}

/******************************************************************************
* Create a FileResourceCreator instance.
*/
ResourceCreator* FileResourceDataModel::createResourceCreator(KAlarmCal::CalEvent::Type defaultType, QWidget* parent)
{
    return new FileResourceCreator(defaultType, parent);
}

/******************************************************************************
* Update a resource's backend calendar file to the current KAlarm format.
*/
void FileResourceDataModel::updateCalendarToCurrentFormat(Resource& resource, bool ignoreKeepFormat, QObject* parent)
{
    FileResourceCalendarUpdater::updateToCurrentFormat(resource, ignoreKeepFormat, parent);
}

/******************************************************************************
* Create model instances which are dependent on the resource data model type.
*/
ResourceListModel* FileResourceDataModel::createResourceListModel(QObject* parent)
{
    return ResourceListModel::create<FileResourceDataModel>(parent);
}

ResourceFilterCheckListModel* FileResourceDataModel::createResourceFilterCheckListModel(QObject* parent)
{
    return ResourceFilterCheckListModel::create<FileResourceDataModel>(parent);
}

AlarmListModel* FileResourceDataModel::createAlarmListModel(QObject* parent)
{
    return AlarmListModel::create<FileResourceDataModel>(parent);
}

AlarmListModel* FileResourceDataModel::allAlarmListModel()
{
    return AlarmListModel::all<FileResourceDataModel>();
}

TemplateListModel* FileResourceDataModel::createTemplateListModel(QObject* parent)
{
    return TemplateListModel::create<FileResourceDataModel>(parent);
}

TemplateListModel* FileResourceDataModel::allTemplateListModel()
{
    return TemplateListModel::all<FileResourceDataModel>();
}

/******************************************************************************
* Return the number of children of a model index.
*/
int FileResourceDataModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return mResourceNodes.keys().count() - 1;
    const Node* node = reinterpret_cast<Node*>(parent.internalPointer());
    if (node  &&  node->type == Type::Resource)
        return mResourceNodes.value(node->resource()).count();
    return 0;
}

/******************************************************************************
* Return the number of columns for children of a model index.
*/
int FileResourceDataModel::columnCount(const QModelIndex& parent) const
{
    // Although the number of columns differs between resources and events,
    // returning different values here doesn't work. So return the maximum
    // number of columns.
    Q_UNUSED(parent);
    return ColumnCount;
}

/******************************************************************************
* Return the model index for a specified item.
*/
QModelIndex FileResourceDataModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row >= 0  &&  column >= 0)
    {
        if (!parent.isValid())
        {
            if (!column)
            {
                const QVector<Node*>& nodes = mResourceNodes.value(Resource());
                if (row >= 0  &&  row < nodes.count())
                    return createIndex(row, column, nodes[row]);
            }
        }
        else
        {
            if (column < ColumnCount)
            {
                const Node* node = reinterpret_cast<Node*>(parent.internalPointer());
                if (node)
                {
                    Resource resource = node->resource();
                    if (resource.isValid())
                    {
                        const QVector<Node*>& nodes = mResourceNodes.value(resource);
                        if (row < nodes.count())
                            return createIndex(row, column, nodes[row]);
                    }
                }
            }
        }
    }
    return QModelIndex();
}

/******************************************************************************
* Return the model index for the parent of a specified item.
*/
QModelIndex FileResourceDataModel::parent(const QModelIndex& ix) const
{
    const Node* node = reinterpret_cast<Node*>(ix.internalPointer());
    if (node)
    {
        Resource resource = node->parent();
        if (resource.isValid())
        {
            int row = mResources.indexOf(resource);
            if (row >= 0)
                return createIndex(row, 0, mResourceNodes.value(Resource()).at(row));
        }
    }
    return QModelIndex();
}

/******************************************************************************
* Return the indexes which match a data value in the 'start' index column.
*/
QModelIndexList FileResourceDataModel::match(const QModelIndex& start, int role, const QVariant& value, int hits, Qt::MatchFlags flags) const
{
    switch (role)
    {
        case ResourceIdRole:
        {
            QModelIndexList result;
            const ResourceId id = value.toLongLong();
            if (id >= 0)
            {
                const QModelIndex ix = resourceIndex(id);
                if (ix.isValid())
                    result += ix;
            }
            return result;
        }
        case EventIdRole:
        {
            QModelIndexList result;
            const QModelIndex ix = eventIndex(value.toString());
            if (ix.isValid())
                result += ix;
            return result;
        }
        default:
            break;
    }

    return QAbstractItemModel::match(start, role, value, hits, flags);
}

/******************************************************************************
* Return the data for a given role, for a specified item.
*/
QVariant FileResourceDataModel::data(const QModelIndex& ix, int role) const
{
    const Node* node = reinterpret_cast<Node*>(ix.internalPointer());
    if (node)
    {
        const Resource res = node->resource();
        if (!res.isNull())
        {
            bool handled;
            const QVariant value = resourceData(role, res, handled);
            if (handled)
                return value;

            switch (role)
            {
                case Qt::CheckStateRole:
                    return res.enabledTypes() ? Qt::Checked : Qt::Unchecked;
                default:
                    break;
            }
        }
        else
        {
            KAEvent* event = node->event();
            if (event)
            {
                // This is an Event row
                if (role == ParentResourceIdRole)
                    return node->parent().id();
                const Resource res = node->parent();
                bool handled;
                const QVariant value = eventData(role, ix.column(), *event, res, handled);
                if (handled)
                    return value;
            }
        }

        // Return invalid value
        switch (role)
        {
            case ItemTypeRole:
                return static_cast<int>(Type::Error);
            case ResourceIdRole:
            case ParentResourceIdRole:
                return -1;
            case StatusRole:
            default:
                break;
        }
    }
    return QVariant();
}

/******************************************************************************
* Set the data for a given role, for a specified item.
*/
bool FileResourceDataModel::setData(const QModelIndex& ix, const QVariant& value, int role)
{
    // NOTE: need to Q_EMIT dataChanged() whenever something is updated (except via a job).
    const Node* node = reinterpret_cast<Node*>(ix.internalPointer());
    if (!node)
        return false;
    Resource resource = node->resource();
    if (resource.isNull())
    {
        // This is an Event row
        KAEvent* event = node->event();
        if (event  &&  event->isValid())
        {
            switch (role)
            {
                case Qt::EditRole:
                {
                    int row = ix.row();
                    Q_EMIT dataChanged(index(row, 0, ix.parent()), index(row, ColumnCount - 1, ix.parent()));
                    return true;
                }
                default:
                    break;
            }
        }
    }
    return QAbstractItemModel::setData(ix, value, role);
}

/******************************************************************************
* Return data for a column heading.
*/
QVariant FileResourceDataModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    bool handled;
    QVariant value = ResourceDataModelBase::headerData(section, orientation, role, true, handled);
    if (handled)
        return value;
    return QVariant();
}

Qt::ItemFlags FileResourceDataModel::flags(const QModelIndex& ix) const
{
    if (!ix.isValid())
        return Qt::ItemIsEnabled;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
}

/******************************************************************************
* Display a message to the user.
*/
void FileResourceDataModel::slotResourceMessage(ResourceType::MessageType type, const QString& message, const QString& details)
{
    handleResourceMessage(type, message, details);
}

// vim: et sw=4:
