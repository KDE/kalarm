/*
 *  akonadimodel.cpp  -  KAlarm calendar file access using Akonadi
 *  Program:  kalarm
 *  Copyright © 2007-2019 David Jarvie <djarvie@kde.org>
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

#include "akonadimodel.h"
#include "alarmtime.h"
#include "autoqpointer.h"
#include "calendarmigrator.h"
#include "mainwindow.h"
#include "messagebox.h"
#include "preferences.h"
#include "synchtimer.h"
#include "kalarmsettings.h"
#include "kalarmdirsettings.h"

#include <kalarmcal/akonadi.h>
#include <kalarmcal/alarmtext.h>
#include <kalarmcal/compatibilityattribute.h>
#include <kalarmcal/eventattribute.h>

#include <AkonadiCore/agentfilterproxymodel.h>
#include <AkonadiCore/agentinstancecreatejob.h>
#include <AkonadiCore/agentmanager.h>
#include <AkonadiCore/agenttype.h>
#include <AkonadiCore/attributefactory.h>
#include <AkonadiCore/changerecorder.h>
#include <AkonadiCore/collectiondeletejob.h>
#include <AkonadiCore/collectionmodifyjob.h>
#include <AkonadiCore/entitydisplayattribute.h>
#include <AkonadiCore/item.h>
#include <AkonadiCore/itemcreatejob.h>
#include <AkonadiCore/itemmodifyjob.h>
#include <AkonadiCore/itemdeletejob.h>
#include <AkonadiCore/itemfetchscope.h>
#include <AkonadiWidgets/agenttypedialog.h>

#include <KLocalizedString>
#include <kcolorutils.h>

#include <QUrl>
#include <QApplication>
#include <QFileInfo>
#include <QTimer>
#include "kalarm_debug.h"

using namespace Akonadi;
using namespace KAlarmCal;

// Ensure CalendarDataModel::UserRole is valid. CalendarDataModel does not
// include Akonadi headers, so here we check that it has been set to be
// compatible with EntityTreeModel::UserRole.
static_assert((int)CalendarDataModel::UserRole>=(int)Akonadi::EntityTreeModel::UserRole, "CalendarDataModel::UserRole wrong value");

static const Collection::Rights writableRights = Collection::CanChangeItem | Collection::CanCreateItem | Collection::CanDeleteItem;

//static bool checkItem_true(const Item&) { return true; }

/*=============================================================================
= Class: AkonadiModel
=============================================================================*/

AkonadiModel* AkonadiModel::mInstance = nullptr;
int           AkonadiModel::mTimeHourPos = -2;

/******************************************************************************
* Construct and return the singleton.
*/
AkonadiModel* AkonadiModel::instance()
{
    if (!mInstance)
        mInstance = new AkonadiModel(new ChangeRecorder(qApp), qApp);
    return mInstance;
}

/******************************************************************************
* Constructor.
*/
AkonadiModel::AkonadiModel(ChangeRecorder* monitor, QObject* parent)
    : EntityTreeModel(monitor, parent)
    , CalendarDataModel()
    , mMonitor(monitor)
    , mMigrationChecked(false)
    , mMigrating(false)
{
    // Set lazy population to enable the contents of unselected collections to be ignored
    setItemPopulationStrategy(LazyPopulation);

    // Restrict monitoring to collections containing the KAlarm mime types
    monitor->setCollectionMonitored(Collection::root());
    monitor->setResourceMonitored("akonadi_kalarm_resource");
    monitor->setResourceMonitored("akonadi_kalarm_dir_resource");
    monitor->setMimeTypeMonitored(KAlarmCal::MIME_ACTIVE);
    monitor->setMimeTypeMonitored(KAlarmCal::MIME_ARCHIVED);
    monitor->setMimeTypeMonitored(KAlarmCal::MIME_TEMPLATE);
    monitor->itemFetchScope().fetchFullPayload();
    monitor->itemFetchScope().fetchAttribute<EventAttribute>();

    AttributeFactory::registerAttribute<CollectionAttribute>();
    AttributeFactory::registerAttribute<CompatibilityAttribute>();
    AttributeFactory::registerAttribute<EventAttribute>();

    connect(monitor, SIGNAL(collectionChanged(Akonadi::Collection,QSet<QByteArray>)), SLOT(slotCollectionChanged(Akonadi::Collection,QSet<QByteArray>)));
    connect(monitor, &Monitor::collectionRemoved, this, &AkonadiModel::slotCollectionRemoved);
    initCalendarMigrator();
    MinuteTimer::connect(this, SLOT(slotUpdateTimeTo()));
    Preferences::connect(SIGNAL(archivedColourChanged(QColor)), this, SLOT(slotUpdateArchivedColour(QColor)));
    Preferences::connect(SIGNAL(disabledColourChanged(QColor)), this, SLOT(slotUpdateDisabledColour(QColor)));
    Preferences::connect(SIGNAL(holidaysChanged(KHolidays::HolidayRegion)), this, SLOT(slotUpdateHolidays()));
    Preferences::connect(SIGNAL(workTimeChanged(QTime,QTime,QBitArray)), this, SLOT(slotUpdateWorkingHours()));

    connect(this, &AkonadiModel::rowsInserted, this, &AkonadiModel::slotRowsInserted);
    connect(this, &AkonadiModel::rowsAboutToBeRemoved, this, &AkonadiModel::slotRowsAboutToBeRemoved);
    connect(this, &Akonadi::EntityTreeModel::collectionPopulated, this, &AkonadiModel::slotCollectionPopulated);
    connect(monitor, &Monitor::itemChanged, this, &AkonadiModel::slotMonitoredItemChanged);

    connect(ServerManager::self(), &ServerManager::stateChanged, this, &AkonadiModel::checkResources);
    checkResources(ServerManager::state());
}


/******************************************************************************
* Destructor.
*/
AkonadiModel::~AkonadiModel()
{
    if (mInstance == this)
        mInstance = nullptr;
}

/******************************************************************************
* Called when the server manager changes state.
* If it is now running, i.e. the agent manager knows about
* all existing resources.
* Once it is running, i.e. the agent manager knows about
* all existing resources, if necessary migrate any KResources alarm calendars from
* pre-Akonadi versions of KAlarm, or create default Akonadi calendar resources
* if any are missing.
*/
void AkonadiModel::checkResources(ServerManager::State state)
{
    switch (state)
    {
        case ServerManager::Running:
            if (!mMigrationChecked)
            {
                qCDebug(KALARM_LOG) << "AkonadiModel::checkResources: Server running";
                mMigrationChecked = true;
                mMigrating = true;
                CalendarMigrator::execute();
            }
            break;
        case ServerManager::NotRunning:
            qCDebug(KALARM_LOG) << "AkonadiModel::checkResources: Server stopped";
            mMigrationChecked = false;
            mMigrating = false;
            mCollectionAlarmTypes.clear();
            mCollectionRights.clear();
            mCollectionEnabled.clear();
            initCalendarMigrator();
            Q_EMIT serverStopped();
            break;
        default:
            break;
    }
}

/******************************************************************************
* Initialise the calendar migrator so that it can be run (either for the first
* time, or again).
*/
void AkonadiModel::initCalendarMigrator()
{
    CalendarMigrator::reset();
    connect(CalendarMigrator::instance(), &CalendarMigrator::creating,
                                          this, &AkonadiModel::slotCollectionBeingCreated);
    connect(CalendarMigrator::instance(), &QObject::destroyed, this, &AkonadiModel::slotMigrationCompleted);
}

/******************************************************************************
* Return whether calendar migration has completed.
*/
bool AkonadiModel::isMigrationCompleted() const
{
    return mMigrationChecked && !mMigrating;
}

/******************************************************************************
* Return the data for a given role, for a specified item.
*/
QVariant AkonadiModel::data(const QModelIndex& index, int role) const
{
    // First check that it's a role we're interested in - if not, use the base method
    switch (role)
    {
        case Qt::BackgroundRole:
        case Qt::ForegroundRole:
        case Qt::DisplayRole:
        case Qt::TextAlignmentRole:
        case Qt::DecorationRole:
        case Qt::SizeHintRole:
        case Qt::AccessibleTextRole:
        case Qt::ToolTipRole:
        case Qt::CheckStateRole:
        case SortRole:
        case TimeDisplayRole:
        case ValueRole:
        case StatusRole:
        case AlarmActionsRole:
        case AlarmSubActionRole:
        case EnabledRole:
        case CommandErrorRole:
        case BaseColourRole:
            break;
        default:
            return EntityTreeModel::data(index, role);
    }

    const Collection collection = index.data(CollectionRole).value<Collection>();
    if (collection.isValid())
    {
        // This is a Collection row
        // Update the collection's resource with the current collection value.
        Resource& resource = updateResource(collection);
        switch (role)
        {
            case Qt::DisplayRole:
                return resource.displayName();
            case BaseColourRole:
                role = Qt::BackgroundRole;   // get value from EntityTreeModel
                break;
            case Qt::BackgroundRole:
            {
                const QColor colour = resource.backgroundColour();
                if (colour.isValid())
                    return colour;
                break;
            }
            case Qt::ForegroundRole:
                return resource.foregroundColour();
            case Qt::ToolTipRole:
                return tooltip(collection, CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
            default:
                break;
        }
    }
    else
    {
        const Item item = index.data(ItemRole).value<Item>();
        if (item.isValid())
        {
            // This is an Item row
            const QString mime = item.mimeType();
            if ((mime != KAlarmCal::MIME_ACTIVE  &&  mime != KAlarmCal::MIME_ARCHIVED  &&  mime != KAlarmCal::MIME_TEMPLATE)
            ||  !item.hasPayload<KAEvent>())
                return QVariant();
            switch (role)
            {
                case StatusRole:
                    // Mime type has a one-to-one relationship to event's category()
                    if (mime == KAlarmCal::MIME_ACTIVE)
                        return CalEvent::ACTIVE;
                    if (mime == KAlarmCal::MIME_ARCHIVED)
                        return CalEvent::ARCHIVED;
                    if (mime == KAlarmCal::MIME_TEMPLATE)
                        return CalEvent::TEMPLATE;
                    return QVariant();
                case CommandErrorRole:
                    if (!item.hasAttribute<EventAttribute>())
                        return KAEvent::CMD_NO_ERROR;
                    return item.attribute<EventAttribute>()->commandError();
                default:
                    break;
            }
            const KAEvent event(this->event(item));

            bool calendarColour;
            bool handled;
            const QVariant value = eventData(index, role, event, calendarColour, handled);
            if (handled)
            {
                if (calendarColour)
                {
                    const Collection parent = item.parentCollection();
                    const Resource parentResource = resource(parent.id());
                    const QColor colour = parentResource.backgroundColour();
                    if (colour.isValid())
                        return colour;
                }
                return value;
            }
        }
    }
    return EntityTreeModel::data(index, role);
}

/******************************************************************************
* Set the font to use for all items, or the checked state of one item.
* The font must always be set at initialisation.
*/
bool AkonadiModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return false;
    // NOTE: need to Q_EMIT dataChanged() whenever something is updated (except via a job).
    Item item = index.data(ItemRole).value<Item>();
    if (item.isValid())
    {
        bool updateItem = false;
        switch (role)
        {
            case CommandErrorRole:
            {
                const KAEvent::CmdErrType err = static_cast<KAEvent::CmdErrType>(value.toInt());
                switch (err)
                {
                    case KAEvent::CMD_NO_ERROR:
                    case KAEvent::CMD_ERROR:
                    case KAEvent::CMD_ERROR_PRE:
                    case KAEvent::CMD_ERROR_POST:
                    case KAEvent::CMD_ERROR_PRE_POST:
                    {
                        if (err == KAEvent::CMD_NO_ERROR  &&  !item.hasAttribute<EventAttribute>())
                            return true;   // no change
                        EventAttribute* attr = item.attribute<EventAttribute>(Item::AddIfMissing);
                        if (attr->commandError() == err)
                            return true;   // no change
                        attr->setCommandError(err);
                        updateItem = true;
qCDebug(KALARM_LOG)<<"Item:"<<item.id()<<"  CommandErrorRole ->"<<err;
                        break;
                    }
                    default:
                        return false;
                }
                break;
            }
            default:
qCDebug(KALARM_LOG)<<"Item: passing to EntityTreeModel::setData("<<role<<")";
                break;
        }
        if (updateItem)
        {
            queueItemModifyJob(item);
            return true;
        }
    }

    return EntityTreeModel::setData(index, value, role);
}

/******************************************************************************
* Return the number of columns for either a collection or an item.
*/
int AkonadiModel::entityColumnCount(HeaderGroup group) const
{
    switch (group)
    {
        case CollectionTreeHeaders:
            return 1;
        case ItemListHeaders:
            return ColumnCount;
        default:
            return EntityTreeModel::entityColumnCount(group);
    }
}

/******************************************************************************
* Return data for a column heading.
*/
QVariant AkonadiModel::entityHeaderData(int section, Qt::Orientation orientation, int role, HeaderGroup group) const
{
    bool eventHeaders = false;
    switch (group)
    {
        case ItemListHeaders:
            eventHeaders = true;
            Q_FALLTHROUGH();    // fall through to CollectionTreeHeaders
        case CollectionTreeHeaders:
        {
            bool handled;
            const QVariant value = CalendarDataModel::headerData(section, orientation, role, eventHeaders, handled);
            if (handled)
                return value;
            break;
        }
        default:
            break;
    }
    return EntityTreeModel::entityHeaderData(section, orientation, role, group);
}

/******************************************************************************
* Recursive function to Q_EMIT the dataChanged() signal for all items in a
* specified column range.
*/
void AkonadiModel::signalDataChanged(bool (*checkFunc)(const Item&), int startColumn, int endColumn, const QModelIndex& parent)
{
    int start = -1;
    int end   = -1;
    for (int row = 0, count = rowCount(parent);  row < count;  ++row)
    {
        const QModelIndex ix = index(row, 0, parent);
        const Item item = data(ix, ItemRole).value<Item>();
        const bool isItem = item.isValid();
        if (isItem)
        {
            if ((*checkFunc)(item))
            {
                // For efficiency, Q_EMIT a single signal for each group of
                // consecutive items, rather than a separate signal for each item.
                if (start < 0)
                    start = row;
                end = row;
                continue;
            }
        }
        if (start >= 0)
            Q_EMIT dataChanged(index(start, startColumn, parent), index(end, endColumn, parent));
        start = -1;
        if (!isItem)
            signalDataChanged(checkFunc, startColumn, endColumn, ix);
    }

    if (start >= 0)
        Q_EMIT dataChanged(index(start, startColumn, parent), index(end, endColumn, parent));
}

/******************************************************************************
* Signal every minute that the time-to-alarm values have changed.
*/
static bool checkItem_isActive(const Item& item)
{ return item.mimeType() == KAlarmCal::MIME_ACTIVE; }

void AkonadiModel::slotUpdateTimeTo()
{
    signalDataChanged(&checkItem_isActive, TimeToColumn, TimeToColumn, QModelIndex());
}


/******************************************************************************
* Called when the colour used to display archived alarms has changed.
*/
static bool checkItem_isArchived(const Item& item)
{ return item.mimeType() == KAlarmCal::MIME_ARCHIVED; }

void AkonadiModel::slotUpdateArchivedColour(const QColor&)
{
    qCDebug(KALARM_LOG) << "AkonadiModel::slotUpdateArchivedColour";
    signalDataChanged(&checkItem_isArchived, 0, ColumnCount - 1, QModelIndex());
}

/******************************************************************************
* Called when the colour used to display disabled alarms has changed.
*/
static bool checkItem_isDisabled(const Item& item)
{
    if (item.hasPayload<KAEvent>())
    {
        const KAEvent event = item.payload<KAEvent>();
        if (event.isValid())
            return !event.enabled();
    }
    return false;
}

void AkonadiModel::slotUpdateDisabledColour(const QColor&)
{
    qCDebug(KALARM_LOG) << "AkonadiModel::slotUpdateDisabledColour";
    signalDataChanged(&checkItem_isDisabled, 0, ColumnCount - 1, QModelIndex());
}

/******************************************************************************
* Called when the definition of holidays has changed.
*/
static bool checkItem_excludesHolidays(const Item& item)
{
    if (item.hasPayload<KAEvent>())
    {
        const KAEvent event = item.payload<KAEvent>();
        if (event.isValid()  &&  event.holidaysExcluded())
            return true;
    }
    return false;
}

void AkonadiModel::slotUpdateHolidays()
{
    qCDebug(KALARM_LOG) << "AkonadiModel::slotUpdateHolidays";
    Q_ASSERT(TimeToColumn == TimeColumn + 1);  // signal should be emitted only for TimeTo and Time columns
    signalDataChanged(&checkItem_excludesHolidays, TimeColumn, TimeToColumn, QModelIndex());
}

/******************************************************************************
* Called when the definition of working hours has changed.
*/
static bool checkItem_workTimeOnly(const Item& item)
{
    if (item.hasPayload<KAEvent>())
    {
        const KAEvent event = item.payload<KAEvent>();
        if (event.isValid()  &&  event.workTimeOnly())
            return true;
    }
    return false;
}

void AkonadiModel::slotUpdateWorkingHours()
{
    qCDebug(KALARM_LOG) << "AkonadiModel::slotUpdateWorkingHours";
    Q_ASSERT(TimeToColumn == TimeColumn + 1);  // signal should be emitted only for TimeTo and Time columns
    signalDataChanged(&checkItem_workTimeOnly, TimeColumn, TimeToColumn, QModelIndex());
}

/******************************************************************************
* Called when the command error status of an alarm has changed, to save the new
* status and update the visual command error indication.
*/
void AkonadiModel::updateCommandError(const KAEvent& event)
{
    const QModelIndex ix = itemIndex(mEventIds.value(event.id()).itemId);
    if (ix.isValid())
        setData(ix, QVariant(static_cast<int>(event.commandError())), CommandErrorRole);
}

/******************************************************************************
* Return a collection's tooltip text. The collection's enabled status is
* evaluated for specified alarm types.
*/
QString AkonadiModel::tooltip(const Collection& collection, CalEvent::Types types) const
{
    const Resource resource = mResources.value(collection.id());
    const QString name = QLatin1Char('@') + resource.displayName();   // insert markers for stripping out name
    const QUrl url = resource.location();
    const QString type = QLatin1Char('@') + resource.storageType(false);   // file/directory/URL etc.
    const QString locn = resource.displayLocation();
    const bool inactive = !(resource.enabledTypes() & types);
    const QString readonly = readOnlyTooltip(resource);
    const bool writable = readonly.isEmpty();
    return CalendarDataModel::tooltip(writable, inactive, name, type, locn, readonly);
}

/******************************************************************************
* Remove a collection from Akonadi. The calendar file is not removed.
*/
bool AkonadiModel::removeCollection(Akonadi::Collection::Id collectionId)
{
    Resource resource = mResources.value(collectionId);
    if (!resource.isValid())
        return false;
    qCDebug(KALARM_LOG) << "AkonadiModel::removeCollection:" << collectionId;
    mCollectionsDeleting << collectionId;
    resource.notifyDeletion();
    // Note: CollectionDeleteJob deletes the backend storage also.
    AgentManager* agentManager = AgentManager::self();
    const AgentInstance instance = agentManager->instance(resource.configName());
    if (instance.isValid())
        agentManager->removeInstance(instance);
    return true;
}

/******************************************************************************
* Return whether a collection is currently being deleted.
*/
bool AkonadiModel::isCollectionBeingDeleted(Collection::Id id) const
{
    return mCollectionsDeleting.contains(id);
}

/******************************************************************************
* Reload a collection from Akonadi storage. The backend data is not reloaded.
*/
bool AkonadiModel::reloadResource(const Resource& resource)
{
    if (!resource.isValid())
        return false;
    qCDebug(KALARM_LOG) << "AkonadiModel::reloadResource:" << resource.id();
    Collection collection(resource.id());
    mMonitor->setCollectionMonitored(collection, false);
    mMonitor->setCollectionMonitored(collection, true);
    return true;
}

/******************************************************************************
* Reload a collection from Akonadi storage. The backend data is not reloaded.
*/
void AkonadiModel::reload()
{
    qCDebug(KALARM_LOG) << "AkonadiModel::reload";
    const Collection::List collections = mMonitor->collectionsMonitored();
    for (const Collection& collection : collections)
    {
        mMonitor->setCollectionMonitored(collection, false);
        mMonitor->setCollectionMonitored(collection, true);
    }
}

/******************************************************************************
* Returns the index to a specified event.
*/
QModelIndex AkonadiModel::eventIndex(const KAEvent& event) const
{
    return itemIndex(mEventIds.value(event.id()).itemId);
}

/******************************************************************************
* Returns the index to a specified event.
*/
QModelIndex AkonadiModel::eventIndex(const QString& eventId) const
{
    return itemIndex(mEventIds.value(eventId).itemId);
}

#if 0
/******************************************************************************
* Return all events of a given type belonging to a collection.
*/
KAEvent::List AkonadiModel::events(Akonadi::Collection& collection, CalEvent::Type type) const
{
    KAEvent::List list;
    const QModelIndex ix = modelIndexForCollection(this, collection);
    if (ix.isValid())
        getChildEvents(ix, type, list);
    return list;
}

/******************************************************************************
* Recursive function to append all child Events with a given mime type.
*/
void AkonadiModel::getChildEvents(const QModelIndex& parent, CalEvent::Type type, KAEvent::List& events) const
{
    for (int row = 0, count = rowCount(parent);  row < count;  ++row)
    {
        const QModelIndex ix = index(row, 0, parent);
        const Item item = data(ix, ItemRole).value<Item>();
        if (item.isValid())
        {
            if (item.hasPayload<KAEvent>())
            {
                KAEvent event = item.payload<KAEvent>();
                if (event.isValid()  &&  event.category() == type)
                    events += event;
            }
        }
        else
        {
            const Collection c = ix.data(CollectionRole).value<Collection>();
            if (c.isValid())
                getChildEvents(ix, type, events);
        }
    }
}
#endif

KAEvent AkonadiModel::event(const QString& eventId) const
{
    const QModelIndex ix = eventIndex(eventId);
    if (!ix.isValid())
        return KAEvent();
    return event(ix.data(ItemRole).value<Item>(), ix, nullptr);
}

KAEvent AkonadiModel::event(const QModelIndex& index) const
{
    return event(index.data(ItemRole).value<Item>(), index, nullptr);
}

KAEvent AkonadiModel::event(const Akonadi::Item& item, const QModelIndex& index, Akonadi::Collection* collection) const
{
    if (!item.isValid()  ||  !item.hasPayload<KAEvent>())
        return KAEvent();
    const QModelIndex ix = index.isValid() ? index : itemIndex(item.id());
    if (!ix.isValid())
        return KAEvent();
    KAEvent e = item.payload<KAEvent>();
    if (e.isValid())
    {

        Collection c = data(ix, ParentCollectionRole).value<Collection>();
        // Set collection ID using a const method, to avoid unnecessary copying of KAEvent
        e.setCollectionId_const(c.id());
        if (collection)
            *collection = c;
    }
    return e;
}

#if 0
/******************************************************************************
* Add an event to the default or a user-selected Collection.
*/
AkonadiModel::Result AkonadiModel::addEvent(KAEvent* event, CalEvent::Type type, QWidget* promptParent, bool noPrompt)
{
    qCDebug(KALARM_LOG) << "AkonadiModel::addEvent:" << event->id();

    // Determine parent collection - prompt or use default
    bool cancelled;
    const Collection collection = destination(type, Collection::CanCreateItem, promptParent, noPrompt, &cancelled);
    if (!collection.isValid())
    {
        delete event;
        if (cancelled)
            return Cancelled;
        qCDebug(KALARM_LOG) << "No collection";
        return Failed;
    }
    if (!addEvent(event, collection))
    {
        qCDebug(KALARM_LOG) << "Failed";
        return Failed;    // event was deleted by addEvent()
    }
    return Success;
}
#endif

/******************************************************************************
* Add events to a specified Collection.
* Events which are scheduled to be added to the collection are updated with
* their Akonadi item ID.
* The caller must connect to the itemDone() signal to check whether events
* have been added successfully. Note that the first signal may be emitted
* before this function returns.
* Reply = true if item creation has been scheduled for all events,
*       = false if at least one item creation failed to be scheduled.
*/
bool AkonadiModel::addEvents(const KAEvent::List& events, Resource& resource)
{
    bool ok = true;
    for (KAEvent* event : events)
        ok = ok && addEvent(*event, resource);
    return ok;
}

/******************************************************************************
* Add an event to a specified Collection.
* If the event is scheduled to be added to the collection, it is updated with
* its Akonadi item ID.
* The event's 'updated' flag is cleared.
* The caller must connect to the itemDone() signal to check whether events
* have been added successfully.
* Reply = true if item creation has been scheduled.
*/
bool AkonadiModel::addEvent(KAEvent& event, Resource& resource)
{
    qCDebug(KALARM_LOG) << "AkonadiModel::addEvent: ID:" << event.id();
    Collection& collection = AkonadiResource::collection(resource);
    Item item;
    if (!KAlarmCal::setItemPayload(item, event, collection.contentMimeTypes()))
    {
        qCWarning(KALARM_LOG) << "AkonadiModel::addEvent: Invalid mime type for collection";
        return false;
    }
    // Note that the item ID will be inserted in mEventIds after the Akonadi
    // Item has been created by ItemCreateJob, when slotRowsInserted() is called.
    mEventIds[event.id()] = EventIds(collection.id());
qCDebug(KALARM_LOG)<<"-> item id="<<item.id();
    ItemCreateJob* job = new ItemCreateJob(item, collection);
    connect(job, &ItemCreateJob::result, this, &AkonadiModel::itemJobDone);
    mPendingItemJobs[job] = -1;   // the Item doesn't have an ID yet
    job->start();
    return true;
}

/******************************************************************************
* Update an event in its collection.
* The event retains its existing Akonadi item ID.
* The event's 'updated' flag is cleared.
* The caller must connect to the itemDone() signal to check whether the event
* has been updated successfully.
* Reply = true if item update has been scheduled.
*/
bool AkonadiModel::updateEvent(KAEvent& event)
{
    qCDebug(KALARM_LOG) << "AkonadiModel::updateEvent:" << event.id();
    auto it = mEventIds.constFind(event.id());
    if (it == mEventIds.constEnd())
        return false;
    const Item::Id itemId = it.value().itemId;
qCDebug(KALARM_LOG)<<"item id="<<itemId;
    const QModelIndex ix = itemIndex(itemId);
    if (!ix.isValid())
        return false;
    const Collection collection = ix.data(ParentCollectionRole).value<Collection>();
    Item item = ix.data(ItemRole).value<Item>();
qCDebug(KALARM_LOG)<<"item id="<<item.id()<<", revision="<<item.revision();
    if (!KAlarmCal::setItemPayload(item, event, collection.contentMimeTypes()))
    {
        qCWarning(KALARM_LOG) << "AkonadiModel::updateEvent: Invalid mime type for collection";
        return false;
    }
//    setData(ix, QVariant::fromValue(item), ItemRole);
    queueItemModifyJob(item);
    return true;
}

/******************************************************************************
* Delete an event from its collection.
*/
bool AkonadiModel::deleteEvent(const KAEvent& event)
{
    qCDebug(KALARM_LOG) << "AkonadiModel::deleteEvent:" << event.id();
    auto it = mEventIds.constFind(event.id());
    if (it == mEventIds.constEnd())
        return false;
    const Item::Id       itemId       = it.value().itemId;
    const Collection::Id collectionId = it.value().collectionId;
    const QModelIndex ix = itemIndex(itemId);
    if (!ix.isValid())
        return false;
    if (mCollectionsDeleting.contains(collectionId))
    {
        qCDebug(KALARM_LOG) << "Collection being deleted";
        return true;    // the event's collection is being deleted
    }
    const Item item = ix.data(ItemRole).value<Item>();
    ItemDeleteJob* job = new ItemDeleteJob(item);
    connect(job, &ItemDeleteJob::result, this, &AkonadiModel::itemJobDone);
    mPendingItemJobs[job] = itemId;
    job->start();
    return true;
}

/******************************************************************************
* Queue an ItemModifyJob for execution. Ensure that only one job is
* simultaneously active for any one Item.
*
* This is necessary because we can't call two ItemModifyJobs for the same Item
* at the same time; otherwise Akonadi will detect a conflict and require manual
* intervention to resolve it.
*/
void AkonadiModel::queueItemModifyJob(const Item& item)
{
    qCDebug(KALARM_LOG) << "AkonadiModel::queueItemModifyJob:" << item.id();
    QHash<Item::Id, Item>::Iterator it = mItemModifyJobQueue.find(item.id());
    if (it != mItemModifyJobQueue.end())
    {
        // A job is already queued for this item. Replace the queued item value with the new one.
        qCDebug(KALARM_LOG) << "Replacing previously queued job";
        it.value() = item;
    }
    else
    {
        // There is no job already queued for this item
        if (mItemsBeingCreated.contains(item.id()))
        {
            qCDebug(KALARM_LOG) << "Waiting for item initialisation";
            mItemModifyJobQueue[item.id()] = item;   // wait for item initialisation to complete
        }
        else
        {
            Item newItem = item;
            const Item current = itemById(item.id());    // fetch the up-to-date item
            if (current.isValid())
                newItem.setRevision(current.revision());
            mItemModifyJobQueue[item.id()] = Item();   // mark the queued item as now executing
            ItemModifyJob* job = new ItemModifyJob(newItem);
            job->disableRevisionCheck();
            connect(job, &ItemModifyJob::result, this, &AkonadiModel::itemJobDone);
            mPendingItemJobs[job] = item.id();
            qCDebug(KALARM_LOG) << "Executing Modify job for item" << item.id() << ", revision=" << newItem.revision();
        }
    }
}

/******************************************************************************
* Called when an item job has completed.
* Checks for any error.
* Note that for an ItemModifyJob, the item revision number may not be updated
* to the post-modification value. The next queued ItemModifyJob is therefore
* not kicked off from here, but instead from the slot attached to the
* itemChanged() signal, which has the revision updated.
*/
void AkonadiModel::itemJobDone(KJob* j)
{
    const QHash<KJob*, Item::Id>::iterator it = mPendingItemJobs.find(j);
    Item::Id itemId = -1;
    if (it != mPendingItemJobs.end())
    {
        itemId = it.value();
        mPendingItemJobs.erase(it);
    }
    const QByteArray jobClass = j->metaObject()->className();
    qCDebug(KALARM_LOG) << "AkonadiModel::itemJobDone:" << jobClass;
    if (j->error())
    {
        QString errMsg;
        if (jobClass == "Akonadi::ItemCreateJob")
            errMsg = i18nc("@info", "Failed to create alarm.");
        else if (jobClass == "Akonadi::ItemModifyJob")
            errMsg = i18nc("@info", "Failed to update alarm.");
        else if (jobClass == "Akonadi::ItemDeleteJob")
            errMsg = i18nc("@info", "Failed to delete alarm.");
        else
            Q_ASSERT(0);
        qCCritical(KALARM_LOG) << "AkonadiModel::itemJobDone:" << errMsg << itemId << ":" << j->errorString();

        if (itemId >= 0  &&  jobClass == "Akonadi::ItemModifyJob")
        {
            // Execute the next queued job for this item
            const Item current = itemById(itemId);    // fetch the up-to-date item
            checkQueuedItemModifyJob(current);
        }
        // Don't show error details by default, since it's from Akonadi and likely
        // to be too technical for general users.
        KAMessageBox::detailedError(MainWindow::mainMainWindow(), errMsg, j->errorString());
    }
    else
    {
        if (jobClass == "Akonadi::ItemCreateJob")
        {
            // Prevent modification of the item until it is fully initialised.
            // Either slotMonitoredItemChanged() or slotRowsInserted(), or both,
            // will be called when the item is done.
            itemId = static_cast<ItemCreateJob*>(j)->item().id();
            qCDebug(KALARM_LOG) << "AkonadiModel::itemJobDone(ItemCreateJob): item id=" << itemId;
            mItemsBeingCreated << itemId;
        }
    }

/*    if (itemId >= 0  &&  jobClass == "Akonadi::ItemModifyJob")
    {
        const QHash<Item::Id, Item>::iterator it = mItemModifyJobQueue.find(itemId);
        if (it != mItemModifyJobQueue.end())
        {
            if (!it.value().isValid())
                mItemModifyJobQueue.erase(it);   // there are no more jobs queued for the item
        }
    }*/
}

/******************************************************************************
* Check whether there are any ItemModifyJobs waiting for a specified item, and
* if so execute the first one provided its creation has completed. This
* prevents clashes in Akonadi conflicts between simultaneous ItemModifyJobs for
* the same item.
*
* Note that when an item is newly created (e.g. via addEvent()), the KAlarm
* resource itemAdded() function creates an ItemModifyJob to give it a remote
* ID. Until that job is complete, any other ItemModifyJob for the item will
* cause a conflict.
*/
void AkonadiModel::checkQueuedItemModifyJob(const Item& item)
{
    if (mItemsBeingCreated.contains(item.id()))
{qCDebug(KALARM_LOG)<<"Still being created";
        return;    // the item hasn't been fully initialised yet
}
    const QHash<Item::Id, Item>::iterator it = mItemModifyJobQueue.find(item.id());
    if (it == mItemModifyJobQueue.end())
{qCDebug(KALARM_LOG)<<"No jobs queued";
        return;    // there are no jobs queued for the item
}
    Item qitem = it.value();
    if (!qitem.isValid())
    {
        // There is no further job queued for the item, so remove the item from the list
qCDebug(KALARM_LOG)<<"No more jobs queued";
        mItemModifyJobQueue.erase(it);
    }
    else
    {
        // Queue the next job for the Item, after updating the Item's
        // revision number to match that set by the job just completed.
        qitem.setRevision(item.revision());
        mItemModifyJobQueue[item.id()] = Item();   // mark the queued item as now executing
        ItemModifyJob* job = new ItemModifyJob(qitem);
        job->disableRevisionCheck();
        connect(job, &ItemModifyJob::result, this, &AkonadiModel::itemJobDone);
        mPendingItemJobs[job] = qitem.id();
        qCDebug(KALARM_LOG) << "Executing queued Modify job for item" << qitem.id() << ", revision=" << qitem.revision();
    }
}

/******************************************************************************
* Called when rows have been inserted into the model.
*/
void AkonadiModel::slotRowsInserted(const QModelIndex& parent, int start, int end)
{
    qCDebug(KALARM_LOG) << "AkonadiModel::slotRowsInserted:" << start << "-" << end << "(parent =" << parent << ")";
    for (int row = start;  row <= end;  ++row)
    {
        const QModelIndex ix = index(row, 0, parent);
        const Collection collection = ix.data(CollectionRole).value<Collection>();
        if (collection.isValid())
        {
            // A collection has been inserted. Create a new resource to hold it.
            qCDebug(KALARM_LOG) << "Collection" << collection.id() << collection.name();
            Resource& resource = updateResource(collection);
            // Ignore it if it isn't owned by a valid Akonadi resource.
            if (resource.isValid())
            {
                const QSet<QByteArray> attrs{ CollectionAttribute::name() };
                setCollectionChanged(resource, collection, attrs, true);
                Q_EMIT resourceAdded(resource);

                if (!mCollectionsBeingCreated.contains(collection.remoteId())
                &&  (collection.rights() & writableRights) == writableRights)
                {
                    // Update to current KAlarm format if necessary, and if the user agrees
                    CalendarMigrator::updateToCurrentFormat(resource, false, MainWindow::mainMainWindow());
                }
            }
        }
        else
        {
            // An item has been inserted
            const Item item = ix.data(ItemRole).value<Item>();
            if (item.isValid())
            {
                qCDebug(KALARM_LOG) << "item id=" << item.id() << ", revision=" << item.revision();
                if (mItemsBeingCreated.removeAll(item.id()))   // the new item has now been initialised
                    checkQueuedItemModifyJob(item);    // execute the next job queued for the item
            }
        }
    }
    const EventList events = eventList(parent, start, end, true);
    if (!events.isEmpty())
        Q_EMIT eventsAdded(events);
}

/******************************************************************************
* Called when rows are about to be removed from the model.
*/
void AkonadiModel::slotRowsAboutToBeRemoved(const QModelIndex& parent, int start, int end)
{
    qCDebug(KALARM_LOG) << "AkonadiModel::slotRowsAboutToBeRemoved:" << start << "-" << end << "(parent =" << parent << ")";
    const EventList events = eventList(parent, start, end, false);
    if (!events.isEmpty())
    {
        for (const Event& event : events)
            qCDebug(KALARM_LOG) << "Collection:" << event.collection.id() << ", Event ID:" << event.event.id();
        Q_EMIT eventsToBeRemoved(events);
    }
}

/******************************************************************************
* Return a list of KAEvent/Collection pairs for a given range of rows.
* If 'inserted' is true, the events will be added to mEventIds; if false,
* the events will be removed from mEventIds.
*/
AkonadiModel::EventList AkonadiModel::eventList(const QModelIndex& parent, int start, int end, bool inserted)
{
    EventList events;
    for (int row = start;  row <= end;  ++row)
    {
        Collection c;
        const QModelIndex ix = index(row, 0, parent);
        const Item item = ix.data(ItemRole).value<Item>();
        const KAEvent evnt = event(item, ix, &c);
        if (evnt.isValid())
        {
            events += Event(evnt, c);
            if (inserted)
                mEventIds[evnt.id()] = EventIds(c.id(), item.id());
            else
                mEventIds.remove(evnt.id());
        }
    }
    return events;
}

/******************************************************************************
* Called when a monitored collection has changed.
* Updates the collection held by the collection's resource, and notifies
* changes of interest.
*/
void AkonadiModel::slotCollectionChanged(const Akonadi::Collection& c, const QSet<QByteArray>& attrNames)
{
    qCDebug(KALARM_LOG) << "AkonadiModel::slotCollectionChanged:" << c.id() << attrNames;
    Resource& resource = updateResource(c);
    setCollectionChanged(resource, c, attrNames, false);
}

/******************************************************************************
* Called when a monitored collection's properties or content have changed.
* Optionally emits a signal if properties of interest have changed.
*/
void AkonadiModel::setCollectionChanged(Resource& resource, const Collection& collection, const QSet<QByteArray>& attributeNames, bool rowInserted)
{
    // Check for a read/write permission change
    const Collection::Rights oldRights = mCollectionRights.value(collection.id(), Collection::AllRights);
    const Collection::Rights newRights = collection.rights() & writableRights;
    if (newRights != oldRights)
    {
        qCDebug(KALARM_LOG) << "AkonadiModel::setCollectionChanged:" << collection.id() << ": rights ->" << newRights;
        mCollectionRights[collection.id()] = newRights;
        Q_EMIT resourceStatusChanged(resource, ReadOnly, (newRights != writableRights), rowInserted);
    }

    // Check for a change in content mime types
    // (e.g. when a collection is first created at startup).
    const CalEvent::Types oldAlarmTypes = mCollectionAlarmTypes.value(collection.id(), CalEvent::EMPTY);
    const CalEvent::Types newAlarmTypes = resource.alarmTypes();
    if (newAlarmTypes != oldAlarmTypes)
    {
        qCDebug(KALARM_LOG) << "AkonadiModel::setCollectionChanged:" << collection.id() << ": alarm types ->" << newAlarmTypes;
        mCollectionAlarmTypes[collection.id()] = newAlarmTypes;
        Q_EMIT resourceStatusChanged(resource, AlarmTypes, static_cast<int>(newAlarmTypes), rowInserted);
    }

    // Check for the collection being enabled/disabled
    if (attributeNames.contains(CollectionAttribute::name())  &&  collection.hasAttribute<CollectionAttribute>())
    {
        // Enabled/disabled can only be set by KAlarm (not the resource), so if the
        // attibute doesn't exist, it is ignored.
        const CalEvent::Types newEnabled = collection.attribute<CollectionAttribute>()->enabled();
        handleEnabledChange(resource, newEnabled, rowInserted);
    }

    // Check for the backend calendar format changing
    if (attributeNames.contains(CompatibilityAttribute::name()))
    {
        // Update to current KAlarm format if necessary, and if the user agrees
        qCDebug(KALARM_LOG) << "AkonadiModel::setCollectionChanged: CompatibilityAttribute";
        CalendarMigrator::updateToCurrentFormat(resource, false, MainWindow::mainMainWindow());
    }

    if (mMigrating)
    {
        mCollectionIdsBeingCreated.removeAll(collection.id());
        if (mCollectionsBeingCreated.isEmpty() && mCollectionIdsBeingCreated.isEmpty()
        &&  CalendarMigrator::completed())
        {
            qCDebug(KALARM_LOG) << "AkonadiModel::setCollectionChanged: Migration completed";
            mMigrating = false;
            Q_EMIT migrationCompleted();
        }
    }
}

/******************************************************************************
* Called by a resource to notify that its status has changed.
*/
void AkonadiModel::notifySettingsChanged(AkonadiResource* res, Change change)
{
    AkonadiModel* model = AkonadiModel::instance();
    auto it = model->mResources.find(res->id());
    if (it != model->mResources.end())
    {
        Resource resource = it.value();
        if (change == Enabled)
        {
            const CalEvent::Types newEnabled = resource.enabledTypes();
            instance()->handleEnabledChange(resource, newEnabled, false);
        }
    }
}

/******************************************************************************
* Emit a signal if a resource's enabled state has changed.
*/
void AkonadiModel::handleEnabledChange(Resource& resource, CalEvent::Types newEnabled, bool rowInserted)
{
    static bool firstEnabled = true;
    const CalEvent::Types oldEnabled = mCollectionEnabled.value(resource.id(), CalEvent::EMPTY);
    if (firstEnabled  ||  newEnabled != oldEnabled)
    {
        qCDebug(KALARM_LOG) << "AkonadiModel::handleEnabledChange:" << resource.id() << ": enabled ->" << newEnabled;
        firstEnabled = false;
        mCollectionEnabled[resource.id()] = newEnabled;
        Q_EMIT resourceStatusChanged(resource, Enabled, static_cast<int>(newEnabled), rowInserted);
    }
}

/******************************************************************************
* Called when a monitored collection is removed.
*/
void AkonadiModel::slotCollectionRemoved(const Collection& collection)
{
    const Collection::Id id = collection.id();
    qCDebug(KALARM_LOG) << "AkonadiModel::slotCollectionRemoved:" << id;
    mResources.remove(collection.id());
    mCollectionRights.remove(id);
    mCollectionsDeleting.removeAll(id);
    Q_EMIT collectionDeleted(id);
}

/******************************************************************************
* Called when a collection creation is about to start, or has completed.
*/
void AkonadiModel::slotCollectionBeingCreated(const QString& path, Akonadi::Collection::Id id, bool finished)
{
    if (finished)
    {
        mCollectionsBeingCreated.removeAll(path);
        mCollectionIdsBeingCreated << id;
    }
    else
        mCollectionsBeingCreated << path;
}

/******************************************************************************
* Called when a collection has been populated.
*/
void AkonadiModel::slotCollectionPopulated(Akonadi::Collection::Id)
{
    if (isFullyPopulated())
    {
        // All collections have now been populated.
        Q_EMIT collectionsPopulated();
        // Prevent the signal being emitted more than once.
        disconnect(this, &Akonadi::EntityTreeModel::collectionPopulated, this, &AkonadiModel::slotCollectionPopulated);
    }
}

/******************************************************************************
* Called when calendar migration has completed.
*/
void AkonadiModel::slotMigrationCompleted()
{
    if (mCollectionsBeingCreated.isEmpty() && mCollectionIdsBeingCreated.isEmpty())
    {
        qCDebug(KALARM_LOG) << "AkonadiModel: Migration completed";
        mMigrating = false;
        Q_EMIT migrationCompleted();
    }
}

/******************************************************************************
* Called when an item in the monitored collections has changed.
*/
void AkonadiModel::slotMonitoredItemChanged(const Akonadi::Item& item, const QSet<QByteArray>&)
{
    qCDebug(KALARM_LOG) << "AkonadiModel::slotMonitoredItemChanged: item id=" << item.id() << ", revision=" << item.revision();
    mItemsBeingCreated.removeAll(item.id());   // the new item has now been initialised
    checkQueuedItemModifyJob(item);    // execute the next job queued for the item

    KAEvent evnt = event(item);
    if (!evnt.isValid())
        return;
    const QModelIndexList indexes = modelIndexesForItem(this, item);
    for (const QModelIndex& index : indexes)
    {
        if (index.isValid())
        {
            // Wait to ensure that the base EntityTreeModel has processed the
            // itemChanged() signal first, before we Q_EMIT eventChanged().
            Collection c = data(index, ParentCollectionRole).value<Collection>();
            evnt.setCollectionId(c.id());
            mPendingEventChanges.enqueue(Event(evnt, c));
            QTimer::singleShot(0, this, &AkonadiModel::slotEmitEventChanged);
            break;
        }
    }
}

/******************************************************************************
* Called to Q_EMIT a signal when an event in the monitored collections has
* changed.
*/
void AkonadiModel::slotEmitEventChanged()
{
    while (!mPendingEventChanges.isEmpty())
    {
        Q_EMIT eventChanged(mPendingEventChanges.dequeue());
    }
}

/******************************************************************************
* Refresh the specified Collection with up to date data.
* Return: true if successful, false if collection not found.
*/
bool AkonadiModel::refresh(Akonadi::Collection& collection) const
{
    const QModelIndex ix = modelIndexForCollection(this, collection);
    if (!ix.isValid())
        return false;
    collection = ix.data(CollectionRole).value<Collection>();

    // Also update our own copy of the collection.
    updateResource(collection);
    return true;
}

#if 0
/******************************************************************************
* Refresh the specified Item with up to date data.
* Return: true if successful, false if item not found.
*/
bool AkonadiModel::refresh(Akonadi::Item& item) const
{
    const QModelIndexList ixs = modelIndexesForItem(this, item);
    if (ixs.isEmpty()  ||  !ixs[0].isValid())
        return false;
    item = ixs[0].data(ItemRole).value<Item>();
    return true;
}
#endif

/******************************************************************************
* Return the AkonadiResource object for a collection ID.
*/
Resource AkonadiModel::resource(Collection::Id id) const
{
    return mResources.value(id, AkonadiResource::nullResource());
}

/******************************************************************************
* Find the collection containing the specified event.
*/
Resource AkonadiModel::resource(const KAEvent& event) const
{
    const Collection::Id id = mEventIds.value(event.id()).collectionId;
    return mResources.value(id, AkonadiResource::nullResource());
}

/******************************************************************************
* Find the collection containing the specified event.
*/
Resource AkonadiModel::resourceForEvent(const QString& eventId) const
{
    const Collection::Id id = mEventIds.value(eventId).collectionId;
    return mResources.value(id, AkonadiResource::nullResource());
}

/******************************************************************************
* Return the resource at a specified index, with up to date data.
*/
Resource AkonadiModel::resource(const QModelIndex& ix) const
{
    return mResources.value(ix.data(CollectionIdRole).toLongLong(), AkonadiResource::nullResource());
}

/******************************************************************************
* Find the QModelIndex of a resource.
*/
QModelIndex AkonadiModel::resourceIndex(const Resource& resource) const
{
    const Collection& collection = AkonadiResource::collection(resource);
    const QModelIndex ix = modelIndexForCollection(this, collection);
    if (!ix.isValid())
        return QModelIndex();
    return ix;
}

/******************************************************************************
* Find the QModelIndex of a resource with a given ID.
*/
QModelIndex AkonadiModel::resourceIndex(Akonadi::Collection::Id id) const
{
    const QModelIndex ix = modelIndexForCollection(this, Collection(id));
    if (!ix.isValid())
        return QModelIndex();
    return ix;
}

/******************************************************************************
* Find the ID of the collection containing the specified event.
*/
Collection::Id AkonadiModel::resourceIdForEvent(const QString& eventId) const
{
    return mEventIds.value(eventId).collectionId;
}

/******************************************************************************
* Return a reference to the collection held in a Resource. This is the
* definitive copy of the collection used by this model.
* Return: the collection held by the model, or null if not found.
*/
Collection* AkonadiModel::collection(const Resource& resource) const
{
    auto it = mResources.find(resource.id());
    if (it != mResources.end())
    {
        Collection& c = AkonadiResource::collection(it.value());
        if (c.isValid())
            return &c;
    }
    return nullptr;
}

/******************************************************************************
* Find the QModelIndex of an item.
*/
QModelIndex AkonadiModel::itemIndex(const Akonadi::Item& item) const
{
    const QModelIndexList ixs = modelIndexesForItem(this, item);
    if (ixs.isEmpty()  ||  !ixs[0].isValid())
        return QModelIndex();
    return ixs[0];
}

/******************************************************************************
* Return the up to date item with the specified Akonadi ID.
*/
Item AkonadiModel::itemById(Item::Id id) const
{
    const QModelIndexList ixs = modelIndexesForItem(this, Item(id));
    if (ixs.isEmpty()  ||  !ixs[0].isValid())
        return Item();
    return ixs[0].data(ItemRole).value<Item>();
}

/******************************************************************************
* Update the resource which holds a given Collection, by copying the Collection
* value into it. If there is no resource, a new resource is created.
* Param: collection - this should have been fetched from the model to ensure
*                     that its value is up to date.
*/
Resource& AkonadiModel::updateResource(const Collection& collection) const
{
    auto it = mResources.find(collection.id());
    if (it != mResources.end())
    {
        Collection& resourceCol = AkonadiResource::collection(it.value());
        if (&collection != &resourceCol)
            resourceCol = collection;
    }
    else
    {
        // Create a new resource for the collection.
        it = mResources.insert(collection.id(), Resource(new AkonadiResource(collection)));
    }
    return it.value();
}

/******************************************************************************
* Find the collection containing the specified Akonadi item ID.
*/
Collection AkonadiModel::collectionForItem(Item::Id id) const
{
    const QModelIndex ix = itemIndex(id);
    if (!ix.isValid())
        return Collection();
    return ix.data(ParentCollectionRole).value<Collection>();
}

void AkonadiModel::notifyResourceError(AkonadiResource*, const QString& message, const QString& details)
{
    KAMessageBox::detailedError(MainWindow::mainMainWindow(), message, details);
}

// vim: et sw=4:
