/*
 *  eventmodel.cpp  -  model containing flat list of events
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

#include "eventmodel.h"

#include "resourcedatamodelbase.h"
#include "resources.h"
#include "preferences.h"
#include "lib/messagebox.h"
#include "kalarm_debug.h"

#include <KLocalizedString>

#include <QApplication>
#include <QMouseEvent>
#include <QHelpEvent>
#include <QToolTip>

/*============================================================================*/

EventListModel::EventListModel(CalEvent::Types types, QObject* parent)
    : QSortFilterProxyModel(parent)
    , mAlarmTypes(types == CalEvent::EMPTY ? CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE : types)
{
    setSourceModel(new KDescendantsProxyModel(this));
    setSortRole(ResourceDataModelBase::SortRole);
    setDynamicSortFilter(true);
    connect(this, &QAbstractItemModel::rowsInserted, this, &EventListModel::slotRowsInserted);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &EventListModel::slotRowsRemoved);
    connect(Resources::instance(), &Resources::resourcePopulated,
                             this, &QSortFilterProxyModel::invalidate);
    connect(Resources::instance(), &Resources::settingsChanged,
                             this, &EventListModel::resourceSettingsChanged);
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
    KDescendantsProxyModel* proxyModel = static_cast<KDescendantsProxyModel*>(sourceModel());
    const QModelIndex dataIndex = proxyModel->mapToSource(mapToSource(index));
    return (*mEventFunction)(dataIndex);
}

/******************************************************************************
* Return the index to a specified event.
*/
QModelIndex EventListModel::eventIndex(const QString& eventId) const
{
    KDescendantsProxyModel* proxyModel = static_cast<KDescendantsProxyModel*>(sourceModel());
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

    QModelIndexList matches;
    const QModelIndexList indexes = sourceModel()->match(mapToSource(start), role, value, hits, flags);
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

bool EventListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    // Get the resource which contains this event.
    KDescendantsProxyModel* proxyModel = static_cast<KDescendantsProxyModel*>(sourceModel());
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
* Called when rows have been inserted into the model.
*/
void EventListModel::slotRowsInserted()
{
    if (!mHaveEvents  &&  rowCount())
    {
        mHaveEvents = true;
        Q_EMIT haveEventsStatus(true);
    }
}

/******************************************************************************
* Called when rows have been deleted from the model.
*/
void EventListModel::slotRowsRemoved()
{
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

bool AlarmListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (!EventListModel::filterAcceptsRow(sourceRow, sourceParent))
        return false;
    if (mFilterTypes == CalEvent::EMPTY)
        return false;
    const int type = sourceModel()->data(sourceModel()->index(sourceRow, 0, sourceParent), ResourceDataModelBase::StatusRole).toInt();
    return static_cast<CalEvent::Type>(type) & mFilterTypes;
}

bool AlarmListModel::filterAcceptsColumn(int sourceCol, const QModelIndex& ix) const
{
    if (!EventListModel::filterAcceptsColumn(sourceCol, ix))
        return false;
    return (sourceCol != ResourceDataModelBase::TemplateNameColumn);
}

QVariant AlarmListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal)
    {
        if (section < 0  ||  section >= ColumnCount)
            return QVariant();
    }
    return EventListModel::headerData(section, orientation, role);
}


/*=============================================================================
= Class: TemplateListModel
= Filter proxy model containing all alarm templates, optionally for specified
= alarm action types (display, email, etc.) in enabled resources.
=============================================================================*/
TemplateListModel* TemplateListModel::mAllInstance = nullptr;

TemplateListModel::TemplateListModel(QObject* parent)
    : EventListModel(CalEvent::TEMPLATE, parent)
    , mActionsEnabled(KAEvent::ACT_ALL)
    , mActionsFilter(KAEvent::ACT_ALL)
{
}

TemplateListModel::~TemplateListModel()
{
    if (this == mAllInstance)
        mAllInstance = nullptr;
}

void TemplateListModel::setAlarmActionFilter(KAEvent::Actions types)
{
    // Ensure that the filter isn't applied to the 'all' instance.
    if (this != mAllInstance  &&  types != mActionsFilter)
    {
        mActionsFilter = types;
        invalidateFilter();
    }
}

void TemplateListModel::setAlarmActionsEnabled(KAEvent::Actions types)
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
    if (mActionsFilter == KAEvent::ACT_ALL)
        return true;
    const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const KAEvent::Actions actions = static_cast<KAEvent::Actions>(sourceModel()->data(sourceIndex, ResourceDataModelBase::AlarmActionsRole).toInt());
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
                return QVariant();
        }
    }
    return EventListModel::headerData(section, orientation, role);
}

Qt::ItemFlags TemplateListModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags f = sourceModel()->flags(mapToSource(index));
    if (mActionsEnabled == KAEvent::ACT_ALL)
        return f;
    const KAEvent::Actions actions = static_cast<KAEvent::Actions>(EventListModel::data(index, ResourceDataModelBase::AlarmActionsRole).toInt());
    if (!(actions & mActionsEnabled))
        f = static_cast<Qt::ItemFlags>(f & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
    return f;
}

// vim: et sw=4:
