/*
 *  itemlistmodel.cpp  -  Akonadi item models
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

#include "itemlistmodel.h"
#include "collectionmodel.h"

#include <kalarmcal/kaevent.h>

#include <kselectionproxymodel.h>

using namespace Akonadi;


/*=============================================================================
= Class: ItemListModel
= Filter proxy model containing all items (alarms/templates) of specified mime
= types in enabled collections.
=============================================================================*/
ItemListModel::ItemListModel(CalEvent::Types allowed, QObject* parent)
    : EntityMimeTypeFilterModel(parent)
    , mAllowedTypes(allowed)
    , mHaveEvents(false)
{
    KSelectionProxyModel* selectionModel = new KSelectionProxyModel(CollectionControlModel::instance()->selectionModel(), this);
    selectionModel->setSourceModel(AkonadiModel::instance());
    selectionModel->setFilterBehavior(KSelectionProxyModel::ChildrenOfExactSelection);
    setSourceModel(selectionModel);

    addMimeTypeExclusionFilter(Collection::mimeType());
    setHeaderGroup(EntityTreeModel::ItemListHeaders);
    if (allowed)
    {
        const QStringList mimeTypes = CalEvent::mimeTypes(allowed);
        for (const QString& mime : mimeTypes)
            addMimeTypeInclusionFilter(mime);
    }
    setHeaderGroup(EntityTreeModel::ItemListHeaders);
    setSortRole(AkonadiModel::SortRole);
    setDynamicSortFilter(true);
    connect(this, &ItemListModel::rowsInserted, this, &ItemListModel::slotRowsInserted);
    connect(this, &ItemListModel::rowsRemoved, this, &ItemListModel::slotRowsRemoved);
    connect(AkonadiModel::instance(), &AkonadiModel::resourceStatusChanged,
                                this, &ItemListModel::resourceStatusChanged);
}

int ItemListModel::columnCount(const QModelIndex& /*parent*/) const
{
    return AkonadiModel::ColumnCount;
}

/******************************************************************************
* Called when rows have been inserted into the model.
*/
void ItemListModel::slotRowsInserted()
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
void ItemListModel::slotRowsRemoved()
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
void ItemListModel::resourceStatusChanged(const Resource& resource, AkonadiModel::Change change, const QVariant&, bool inserted)
{
    Q_UNUSED(inserted);
    if (!resource.isValid())
        return;
    if (change == AkonadiModel::Enabled)
    {
        // Ensure that items for a newly enabled resource are always ordered
        // correctly. Note that invalidateFilter() is not adequate for this.
        invalidate();
    }
}

bool ItemListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (!EntityMimeTypeFilterModel::filterAcceptsRow(sourceRow, sourceParent))
        return false;
    // Get the alarm type of the item
    const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const CalEvent::Type type = static_cast<CalEvent::Type>(sourceModel()->data(sourceIndex, AkonadiModel::StatusRole).toInt());
    const Collection parent = sourceIndex.data(AkonadiModel::ParentCollectionRole).value<Collection>();
    const Resource parentResource = AkonadiModel::instance()->resource(parent.id());
    return parentResource.isEnabled(type);
}

Qt::ItemFlags ItemListModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::ItemIsEnabled;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
}

/******************************************************************************
* Return the index to a specified event.
*/
QModelIndex ItemListModel::eventIndex(const QString& eventId) const
{
    const QModelIndex ix = AkonadiModel::instance()->eventIndex(eventId);
    return mapFromSource(static_cast<KSelectionProxyModel*>(sourceModel())->mapFromSource(ix));
}

/******************************************************************************
* Return the event in a specified row.
*/
KAEvent ItemListModel::event(int row) const
{
    return event(index(row, 0));
}

/******************************************************************************
* Return the event referred to by an index.
*/
KAEvent ItemListModel::event(const QModelIndex& index) const
{
    return static_cast<AkonadiModel*>(sourceModel())->event(mapToSource(index));
}

/******************************************************************************
* Check whether the model contains any events.
*/
bool ItemListModel::haveEvents() const
{
    return rowCount();
}


/*=============================================================================
= Class: AlarmListModel
= Filter proxy model containing all alarms (not templates) of specified mime
= types in enabled collections.
Equivalent to AlarmListFilterModel
=============================================================================*/
AlarmListModel* AlarmListModel::mAllInstance = nullptr;

AlarmListModel::AlarmListModel(QObject* parent)
    : ItemListModel(CalEvent::ACTIVE | CalEvent::ARCHIVED, parent)
    , mFilterTypes(CalEvent::ACTIVE | CalEvent::ARCHIVED)
{
}

AlarmListModel::~AlarmListModel()
{
    if (this == mAllInstance)
        mAllInstance = nullptr;
}

AlarmListModel* AlarmListModel::all()
{
    if (!mAllInstance)
    {
        mAllInstance = new AlarmListModel(AkonadiModel::instance());
        mAllInstance->sort(TimeColumn, Qt::AscendingOrder);
    }
    return mAllInstance;
}

void AlarmListModel::setEventTypeFilter(CalEvent::Types types)
{
    // Ensure that the filter isn't applied to the 'all' instance, and that
    // 'types' doesn't include any disallowed alarm types
    if (!types)
        types = includedTypes();
    if (this != mAllInstance
    &&  types != mFilterTypes  &&  (types & includedTypes()) == types)
    {
        mFilterTypes = types;
        invalidateFilter();
    }
}

bool AlarmListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (!ItemListModel::filterAcceptsRow(sourceRow, sourceParent))
        return false;
    if (mFilterTypes == CalEvent::EMPTY)
        return false;
    const int type = sourceModel()->data(sourceModel()->index(sourceRow, 0, sourceParent), CalendarDataModel::StatusRole).toInt();
    return static_cast<CalEvent::Type>(type) & mFilterTypes;
}

bool AlarmListModel::filterAcceptsColumn(int sourceCol, const QModelIndex&) const
{
    return (sourceCol != CalendarDataModel::TemplateNameColumn);
}

QVariant AlarmListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal)
    {
        if (section < 0  ||  section >= ColumnCount)
            return QVariant();
    }
    return ItemListModel::headerData(section, orientation, role);
}


/*=============================================================================
= Class: TemplateListModel
= Filter proxy model containing all alarm templates for specified alarm types
= in enabled collections.
Equivalent to TemplateListFilterModel
=============================================================================*/
TemplateListModel* TemplateListModel::mAllInstance = nullptr;

TemplateListModel::TemplateListModel(QObject* parent)
    : ItemListModel(CalEvent::TEMPLATE, parent)
    , mActionsEnabled(KAEvent::ACT_ALL)
    , mActionsFilter(KAEvent::ACT_ALL)
{
}

TemplateListModel::~TemplateListModel()
{
    if (this == mAllInstance)
        mAllInstance = nullptr;
}

TemplateListModel* TemplateListModel::all()
{
    if (!mAllInstance)
    {
        mAllInstance = new TemplateListModel(AkonadiModel::instance());
        mAllInstance->sort(TemplateNameColumn, Qt::AscendingOrder);
    }
    return mAllInstance;
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
    if (!ItemListModel::filterAcceptsRow(sourceRow, sourceParent))
        return false;
    if (mActionsFilter == KAEvent::ACT_ALL)
        return true;
    const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    const KAEvent::Actions actions = static_cast<KAEvent::Actions>(sourceModel()->data(sourceIndex, CalendarDataModel::AlarmActionsRole).toInt());
    return actions & mActionsFilter;
}

bool TemplateListModel::filterAcceptsColumn(int sourceCol, const QModelIndex&) const
{
    return sourceCol == CalendarDataModel::TemplateNameColumn
       ||  sourceCol == CalendarDataModel::TypeColumn;
}

QVariant TemplateListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal)
    {
        switch (section)
        {
            case TypeColumn:
                section = CalendarDataModel::TypeColumn;
                break;
            case TemplateNameColumn:
                section = CalendarDataModel::TemplateNameColumn;
                break;
            default:
                return QVariant();
        }
    }
    return ItemListModel::headerData(section, orientation, role);
}

Qt::ItemFlags TemplateListModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags f = sourceModel()->flags(mapToSource(index));
    if (mActionsEnabled == KAEvent::ACT_ALL)
        return f;
    const KAEvent::Actions actions = static_cast<KAEvent::Actions>(ItemListModel::data(index, CalendarDataModel::AlarmActionsRole).toInt());
    if (!(actions & mActionsEnabled))
        f = static_cast<Qt::ItemFlags>(f & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
    return f;
}

// vim: et sw=4:
