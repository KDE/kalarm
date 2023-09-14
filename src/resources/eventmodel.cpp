/*
 *  eventmodel.cpp  -  model containing flat list of events
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "eventmodel.h"

#include "resourcedatamodelbase.h"
#include "resources.h"
#include "preferences.h"


/*============================================================================*/

EventListModel::EventListModel(CalEvent::Types types, QObject* parent)
    : QSortFilterProxyModel(parent)
    , mAlarmTypes(types == CalEvent::EMPTY ? CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE : types)
{
    setSourceModel(new KDescendantsProxyModel(this));
    setSortRole(ResourceDataModelBase::SortRole);
    setDynamicSortFilter(true);

    Resources* resources = Resources::instance();
    connect(resources, &Resources::eventsAdded,       this, &EventListModel::slotRowsInserted);
    connect(resources, &Resources::eventsRemoved,     this, &EventListModel::slotRowsRemoved);
    connect(resources, &Resources::resourcePopulated, this, &EventListModel::slotResourcePopulated);
    connect(resources, &Resources::settingsChanged,   this, &EventListModel::resourceSettingsChanged);
}

/******************************************************************************
* Return the event in a specified row.
*/
KAEvent EventListModel::event(int row) const
{
    return event(index(row, 0));
}

/******************************************************************************
* Return the event referred to by an index.
*/
KAEvent EventListModel::event(const QModelIndex& index) const
{
    auto proxyModel = static_cast<KDescendantsProxyModel*>(sourceModel());
    const QModelIndex dataIndex = proxyModel->mapToSource(mapToSource(index));
    return (*mEventFunction)(dataIndex);
}

/******************************************************************************
* Return the event for a given row in the source model.
*/
KAEvent EventListModel::eventForSourceRow(int sourceRow) const
{
    auto proxyModel = static_cast<KDescendantsProxyModel*>(sourceModel());
    const QModelIndex dataIndex = proxyModel->mapToSource(proxyModel->index(sourceRow, 0));
    return (*mEventFunction)(dataIndex);
}

/******************************************************************************
* Return the index to a specified event.
*/
QModelIndex EventListModel::eventIndex(const QString& eventId) const
{
    auto proxyModel = static_cast<KDescendantsProxyModel*>(sourceModel());
    return mapFromSource(proxyModel->mapFromSource(((*mEventIndexFunction)(eventId))));
}

/******************************************************************************
* Check whether the model contains any events.
*/
bool EventListModel::haveEvents() const
{
    return rowCount();
}

int EventListModel::iconWidth()
{
    return ResourceDataModelBase::iconSize().width();
}

bool EventListModel::hasChildren(const QModelIndex& parent) const
{
    return rowCount(parent) > 0;
}

bool EventListModel::canFetchMore(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return false;
}

QModelIndexList EventListModel::match(const QModelIndex& start, int role, const QVariant& value, int hits, Qt::MatchFlags flags) const
{
    if (role < Qt::UserRole)
        return QSortFilterProxyModel::match(start, role, value, hits, flags);

    QModelIndexList matches;           //clazy:exclude=inefficient-qlist
    const QModelIndexList indexes = sourceModel()->match(mapToSource(start), role, value, hits, flags);      //clazy:exclude=inefficient-qlist
    for (const QModelIndex& ix : indexes)
    {
        QModelIndex proxyIndex = mapFromSource(ix);
        if (proxyIndex.isValid())
            matches += proxyIndex;
    }
    return matches;
}

int EventListModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return ResourceDataModelBase::ColumnCount;
}

QVariant EventListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    return static_cast<KDescendantsProxyModel*>(sourceModel())->sourceModel()->headerData(section, orientation, role + mHeaderDataRoleOffset);
}

/******************************************************************************
* Determine whether a source model item is included in this model.
* This also determines whether it is counted in rowCount().
*/
bool EventListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    // Get the resource which contains this event.
    auto proxyModel = static_cast<KDescendantsProxyModel*>(sourceModel());
    const QModelIndex dataIndex = proxyModel->mapToSource(proxyModel->index(sourceRow, 0, sourceParent));
    const QString eventId = dataIndex.data(ResourceDataModelBase::EventIdRole).toString();
    if (eventId.isEmpty())
        return false;   // this row doesn't contain an event
    const ResourceId id = dataIndex.data(ResourceDataModelBase::ParentResourceIdRole).toLongLong();
    if (id < 0)
        return false;   // the parent item isn't a resource
    const Resource resource = Resources::resource(id);
    if (!resource.isValid())
        return false;   // invalidly configured resource

    // Get the event.
    const KAEvent event = resource.event(eventId);
    if (!event.isValid())
        return false;
    if (!(event.category() & mAlarmTypes))
        return false;   // the event has the wrong alarm type
    if (!resource.isEnabled(event.category()))
        return false;   // the resource is disabled for this alarm type
    return true;
}

bool EventListModel::filterAcceptsColumn(int sourceColumn, const QModelIndex& sourceParent) const
{
    if (sourceColumn >= ResourceDataModelBase::ColumnCount)
        return false;
    return QSortFilterProxyModel::filterAcceptsColumn(sourceColumn, sourceParent);
}

/******************************************************************************
* Called when a Resource has been initially populated.
*/
void EventListModel::slotResourcePopulated(Resource& resource)
{
    if (!(resource.enabledTypes() & mAlarmTypes))
        return;    // the resource isn't included in this model

    invalidate();
}

/******************************************************************************
* Called when rows have been inserted into the model.
*
* Note that during initialisation, rows are inserted into the source model
* before they are added to the Resource. Until they have been added to the
* Resource, they will be filtered out by filterAcceptsRow() (and therefore
* omitted by rowCount()), because Resource::event(eventId) will not find them.
* This method is called when the eventsAdded() signal indicates that they have
* now been added to the Resource.
*/
void EventListModel::slotRowsInserted(Resource& resource)
{
    if (!(resource.enabledTypes() & mAlarmTypes))
        return;    // the resource isn't included in this model

    if (!mHaveEvents  &&  rowCount())
    {
        mHaveEvents = true;
        Q_EMIT haveEventsStatus(true);
    }
}

/******************************************************************************
* Called when rows have been deleted from the model.
*/
void EventListModel::slotRowsRemoved(Resource& resource)
{
    if (!(resource.enabledTypes() & mAlarmTypes))
        return;    // the resource isn't included in this model

    if (mHaveEvents  &&  !rowCount())
    {
        mHaveEvents = false;
        Q_EMIT haveEventsStatus(false);
    }
}

/******************************************************************************
* Called when a resource parameter or status has changed.
* If the resource's enabled status has changed, re-filter the list to add or
* remove its alarms.
*/
void EventListModel::resourceSettingsChanged(Resource& resource, ResourceType::Changes change)
{
    if (!resource.isValid())
        return;
    if (change == ResourceType::Enabled)
    {
        // Ensure that items for a newly enabled resource are always ordered
        // correctly. Note that invalidateFilter() is not adequate for this.
        invalidate();
    }
}


/*=============================================================================
= Class: AlarmListModel
= Filter proxy model containing all alarms (not templates) of specified mime
= types in enabled collections.
=============================================================================*/
AlarmListModel* AlarmListModel::mAllInstance = nullptr;

AlarmListModel::AlarmListModel(QObject* parent)
    : EventListModel(CalEvent::ACTIVE | CalEvent::ARCHIVED, parent)
    , mFilterTypes(CalEvent::ACTIVE | CalEvent::ARCHIVED)
{
    // Note: Use Resources::*() signals rather than
    //       ResourceDataModel::rowsAboutToBeRemoved(), since the former is
    //       emitted last. This ensures that mDateFilterCache won't be updated
    //       with the removed events after removing them.
    Resources* resources = Resources::instance();
    connect(resources, &Resources::settingsChanged, this, &AlarmListModel::slotResourceSettingsChanged);
    connect(resources, &Resources::resourceRemoved, this, &AlarmListModel::slotResourceRemoved);
    connect(resources, &Resources::eventUpdated,    this, &AlarmListModel::slotEventUpdated);
    connect(resources, &Resources::eventsRemoved,   this, &AlarmListModel::slotEventsRemoved);
}

AlarmListModel::~AlarmListModel()
{
    if (this == mAllInstance)
        mAllInstance = nullptr;
}

void AlarmListModel::setEventTypeFilter(CalEvent::Types types)
{
    // Ensure that the filter isn't applied to the 'all' instance, and that
    // 'types' doesn't include any alarm types not included in the model.
    types &= alarmTypes();

    if (this != mAllInstance
    &&  types != mFilterTypes)
    {
        mFilterTypes = types;
        invalidateFilter();
    }
}

/******************************************************************************
* Only show alarms which are due on specified dates, or show all alarms.
* The default is to show all alarms.
*/
void AlarmListModel::setDateFilter(const QVector<QDate>& dates, bool force)
{
    const QVector<std::pair<KADateTime, KADateTime>> oldFilterDates = mFilterDates;
    mFilterDates.clear();
    if (!dates.isEmpty())
    {
        // Set the filter to ranges of consecutive dates.
        const KADateTime::Spec timeSpec = Preferences::timeSpec();
        QDate start = dates[0];
        QDate end = start;
        QDate date;
        for (int i = 1, count = dates.count();  i <= count;  ++i)
        {
            if (i < count)
                date = dates[i];
            if (i == count  ||  date > end.addDays(1))
            {
                const KADateTime from(start, QTime(0,0,0), timeSpec);
                const KADateTime to(end, QTime(23,59,0), timeSpec);
                mFilterDates += std::make_pair(from, to);
                start = date;
            }
            end = date;
        }
    }

    if (force  ||  mFilterDates != oldFilterDates)
    {
        mDateFilterCache.clear();   // clear cache of date filter statuses
        // Cause the view to refresh. Note that because date/time values
        // returned by the model will change, invalidateFilter() is not
        // adequate for this.
        invalidate();
    }
}

bool AlarmListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (!EventListModel::filterAcceptsRow(sourceRow, sourceParent))
        return false;
    if (mFilterTypes == CalEvent::EMPTY)
        return false;
    if (!mFilterDates.isEmpty())
    {
        const KAEvent ev = eventForSourceRow(sourceRow);
        if (ev.category() != CalEvent::ACTIVE)
            return false;    // only include active alarms in the filter
        const KADateTime::Spec timeSpec = Preferences::timeSpec();
        const KADateTime now = KADateTime::currentDateTime(timeSpec);
        auto& resourceHash = mDateFilterCache[ev.resourceId()];
        const auto eit = resourceHash.constFind(ev.id());
        bool haveEvent = (eit != resourceHash.constEnd());
        if (haveEvent)
        {
            // Use cached date filter status for this event.
            if (!eit.value().isValid())
                return false;
            if (eit.value() < now)
            {
                resourceHash.erase(eit); // occurrence has passed - check again
                haveEvent = false;
            }
        }
        if (!haveEvent)
        {
            // Determine whether this event is included in the date filter,
            // and cache its status.
            KADateTime occurs;
            for (int i = 0, count = mFilterDates.size();  i < count && !occurs.isValid();  ++i)
            {
                const auto& dateRange = mFilterDates[i];
                KADateTime from = std::max(dateRange.first, now).addSecs(-60);
                while (!occurs.isValid())
                {
                    DateTime nextDt;
                    ev.nextOccurrence(from, nextDt, KAEvent::Repeats::Return);
                    if (!nextDt.isValid())
                    {
                        resourceHash[ev.id()] = KADateTime();
                        return false;
                    }
                    from = nextDt.effectiveKDateTime().toTimeSpec(timeSpec);
                    if (from > dateRange.second)
                    {
                        // The event first occurs after the end of this date range.
                        // Find the next date range which it might be in.
                        while (++i < count  &&  from > mFilterDates[i].second) {}
                        if (i >= count)
                        {
                            resourceHash[ev.id()] = KADateTime();
                            return false;    // the event occurs after all date ranges
                        }
                        if (from < mFilterDates[i].first)
                        {
                            // It is before this next date range.
                            // Find another occurrence and keep checking.
                            --i;
                            break;
                        }
                    }
                    // It lies in this date range.
                    if (!ev.excludedByWorkTimeOrHoliday(from))
                    {
                        occurs = from;
                        break;    // event occurs in this date range
                    }
                    // This occurrence is excluded, so check for another.
                }
            }
            resourceHash[ev.id()] = occurs;
            if (!occurs.isValid())
                return false;
        }
    }
    const int type = sourceModel()->data(sourceModel()->index(sourceRow, 0, sourceParent), ResourceDataModelBase::StatusRole).toInt();
    return static_cast<CalEvent::Type>(type) & mFilterTypes;
}

bool AlarmListModel::filterAcceptsColumn(int sourceCol, const QModelIndex& ix) const
{
    if (!EventListModel::filterAcceptsColumn(sourceCol, ix))
        return false;
    return (sourceCol != ResourceDataModelBase::TemplateNameColumn);
}

/******************************************************************************
* Return the data for a given index from the model.
*/
QVariant AlarmListModel::data(const QModelIndex& ix, int role) const
{
    if (mReplaceBlankName  &&  ix.column() == NameColumn)
    {
        // It's the Name column, and the name is being replaced by the alarm text
        // when the name is blank. Return the alarm text instead for display and
        // tooltip.
        switch (role)
        {
            case Qt::DisplayRole:
            case Qt::ToolTipRole:
                if (EventListModel::data(ix, role).toString().isEmpty())
                {
                    const QModelIndex& ix2 = ix.siblingAtColumn(TextColumn);
                    return EventListModel::data(ix2, role);
                }
                break;
            default:
                break;
        }
    }
    else if (!mFilterDates.isEmpty())
    {
        bool timeCol = false;
        switch (ix.column())
        {
            case TimeColumn:
                timeCol = true;
                Q_FALLTHROUGH();
            case TimeToColumn:
            {
                switch (role)
                {
                    case Qt::DisplayRole:
                    case ResourceDataModelBase::TimeDisplayRole:
                    case ResourceDataModelBase::SortRole:
                    {
                        // Return a value based on the first occurrence in the date filter range.
                        const KAEvent ev = event(ix);
                        const auto rit = mDateFilterCache.constFind(ev.resourceId());
                        if (rit != mDateFilterCache.constEnd())
                        {
                            const auto resourceHash = rit.value();
                            const auto eit = resourceHash.constFind(ev.id());
                            if (eit != resourceHash.constEnd()  &&  eit.value().isValid())
                            {
                                const KADateTime next = eit.value();
                                switch (role)
                                {
                                    case Qt::DisplayRole:
                                        return timeCol ? ResourceDataModelBase::alarmTimeText(next, '0')
                                                       : ResourceDataModelBase::timeToAlarmText(next);
                                    case ResourceDataModelBase::TimeDisplayRole:
                                        if (timeCol)
                                            return ResourceDataModelBase::alarmTimeText(next, '~');
                                        break;
                                    case ResourceDataModelBase::SortRole:
                                        if (timeCol)
                                            return DateTime(next).effectiveKDateTime().toUtc().qDateTime();
                                        else
                                        {
                                            const KADateTime now = KADateTime::currentUtcDateTime();
                                            if (next.isDateOnly())
                                                return now.date().daysTo(next.date()) * 1440;
                                            return (now.secsTo(DateTime(next).effectiveKDateTime()) + 59) / 60;
                                        }
                                    default:
                                        break;
                                }
                            }
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
            default:
                break;
        }
    }
    return EventListModel::data(ix, role);
}

QVariant AlarmListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal)
    {
        if (section < 0  ||  section >= ColumnCount)
            return {};
    }
    return EventListModel::headerData(section, orientation, role);
}

/******************************************************************************
* Called when the enabled or read-only status of a resource has changed.
* If the resource is now disabled, remove its events from the date filter cache.
*/
void AlarmListModel::slotResourceSettingsChanged(Resource& resource, ResourceType::Changes change)
{
    if ((change & ResourceType::Enabled)
    &&  !resource.isEnabled(CalEvent::ACTIVE))
    {
        mDateFilterCache.remove(resource.id());
    }
}

/******************************************************************************
* Called when a resource has been removed.
* Remove all its events from the date filter cache.
*/
void AlarmListModel::slotResourceRemoved(ResourceId id)
{
    mDateFilterCache.remove(id);
}

/******************************************************************************
* Called when an event has been updated.
* Remove it from the date filter cache.
*/
void AlarmListModel::slotEventUpdated(Resource& resource, const KAEvent& event)
{
    auto rit = mDateFilterCache.find(resource.id());
    if (rit != mDateFilterCache.end())
        rit.value().remove(event.id());
}

/******************************************************************************
* Called when events have been removed.
* Remove them from the date filter cache.
*/
void AlarmListModel::slotEventsRemoved(Resource& resource, const QList<KAEvent>& events)
{
    if (!mFilterDates.isEmpty())
    {
        auto rit = mDateFilterCache.find(resource.id());
        if (rit != mDateFilterCache.end())
        {
            for (const KAEvent& event : events)
                rit.value().remove(event.id());
        }
    }
}


/*=============================================================================
= Class: TemplateListModel
= Filter proxy model containing all alarm templates, optionally for specified
= alarm action types (display, email, etc.) in enabled resources.
=============================================================================*/
TemplateListModel* TemplateListModel::mAllInstance = nullptr;

TemplateListModel::TemplateListModel(QObject* parent)
    : EventListModel(CalEvent::TEMPLATE, parent)
    , mActionsEnabled(KAEvent::Action::All)
    , mActionsFilter(KAEvent::Action::All)
{
}

TemplateListModel::~TemplateListModel()
{
    if (this == mAllInstance)
        mAllInstance = nullptr;
}

void TemplateListModel::setAlarmActionFilter(KAEvent::Action types)
{
    // Ensure that the filter isn't applied to the 'all' instance.
    if (this != mAllInstance  &&  types != mActionsFilter)
    {
        mActionsFilter = types;
        invalidateFilter();
    }
}

void TemplateListModel::setAlarmActionsEnabled(KAEvent::Action types)
{
    // Ensure that the setting isn't applied to the 'all' instance.
    if (this != mAllInstance  &&  types != mActionsEnabled)
    {
        mActionsEnabled = types;
        invalidateFilter();
    }
}

bool TemplateListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (!EventListModel::filterAcceptsRow(sourceRow, sourceParent))
        return false;
    if (mActionsFilter == KAEvent::Action::All)
        return true;
    const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const KAEvent::Action actions = static_cast<KAEvent::Action>(sourceModel()->data(sourceIndex, ResourceDataModelBase::AlarmActionsRole).toInt());
    return actions & mActionsFilter;
}

bool TemplateListModel::filterAcceptsColumn(int sourceCol, const QModelIndex&) const
{
    return sourceCol == ResourceDataModelBase::TemplateNameColumn
       ||  sourceCol == ResourceDataModelBase::TypeColumn;
}

QVariant TemplateListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal)
    {
        switch (section)
        {
            case TypeColumn:
                section = ResourceDataModelBase::TypeColumn;
                break;
            case TemplateNameColumn:
                section = ResourceDataModelBase::TemplateNameColumn;
                break;
            default:
                return {};
        }
    }
    return EventListModel::headerData(section, orientation, role);
}

Qt::ItemFlags TemplateListModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags f = sourceModel()->flags(mapToSource(index));
    if (mActionsEnabled == KAEvent::Action::All)
        return f;
    const KAEvent::Action actions = static_cast<KAEvent::Action>(EventListModel::data(index, ResourceDataModelBase::AlarmActionsRole).toInt());
    if (!(actions & mActionsEnabled))
        f = static_cast<Qt::ItemFlags>(f & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
    return f;
}

// vim: et sw=4:
