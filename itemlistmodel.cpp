/*
 *  itemlistmodel.cpp  -  Akonadi item models
 *  Program:  kalarm
 *  Copyright © 2007-2012 by David Jarvie <djarvie@kde.org>
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
    : EntityMimeTypeFilterModel(parent),
      mAllowedTypes(allowed),
      mHaveEvents(false)
{
    KSelectionProxyModel* selectionModel = new KSelectionProxyModel(CollectionControlModel::instance()->selectionModel(), this);
    selectionModel->setSourceModel(AkonadiModel::instance());
    selectionModel->setFilterBehavior(KSelectionProxyModel::ChildrenOfExactSelection);
    setSourceModel(selectionModel);

    addMimeTypeExclusionFilter(Collection::mimeType());
    setHeaderGroup(EntityTreeModel::ItemListHeaders);
    if (allowed)
    {
        QStringList mimeTypes = CalEvent::mimeTypes(allowed);
        foreach (const QString& mime, mimeTypes)
            addMimeTypeInclusionFilter(mime);
    }
    setHeaderGroup(EntityTreeModel::ItemListHeaders);
    setSortRole(AkonadiModel::SortRole);
    setDynamicSortFilter(true);
    connect(this, &ItemListModel::rowsInserted, this, &ItemListModel::slotRowsInserted);
    connect(this, &ItemListModel::rowsRemoved, this, &ItemListModel::slotRowsRemoved);
    connect(AkonadiModel::instance(), SIGNAL(collectionStatusChanged(Akonadi::Collection,AkonadiModel::Change,QVariant,bool)),
                                      SLOT(collectionStatusChanged(Akonadi::Collection,AkonadiModel::Change,QVariant,bool)));
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
        emit haveEventsStatus(true);
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
        emit haveEventsStatus(false);
    }
}

/******************************************************************************
* Called when a collection parameter or status has changed.
* If the collection's enabled status has changed, re-filter the list to add or
* remove its alarms.
*/
void ItemListModel::collectionStatusChanged(const Collection& collection, AkonadiModel::Change change, const QVariant&, bool inserted)
{
    Q_UNUSED(inserted);
    if (!collection.isValid())
        return;
    if (change == AkonadiModel::Enabled)
    {
        // Ensure that items for a newly enabled collection are always ordered
        // correctly. Note that invalidateFilter() is not adequate for this.
        invalidate();
    }
}

bool ItemListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (!EntityMimeTypeFilterModel::filterAcceptsRow(sourceRow, sourceParent))
        return false;
    // Get the alarm type of the item
    QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    CalEvent::Type type = static_cast<CalEvent::Type>(sourceModel()->data(sourceIndex, AkonadiModel::StatusRole).toInt());
    Collection parent = sourceIndex.data(AkonadiModel::ParentCollectionRole).value<Collection>();
    return CollectionControlModel::isEnabled(parent, type);
}

#if 0
QModelIndex ItemListModel::index(int row, int column, const QModelIndex& parent) const
{
    if (parent.isValid())
        return QModelIndex();
    return createIndex(row, column, mEvents[row]);
}

bool ItemListModel::setData(const QModelIndex& ix, const QVariant&, int role)
{
    if (ix.isValid()  &&  role == Qt::EditRole)
    {
//??? update event
        int row = ix.row();
        emit dataChanged(index(row, 0), index(row, AkonadiModel::ColumnCount - 1));
        return true;
    }
    return false;
}
#endif

Qt::ItemFlags ItemListModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::ItemIsEnabled;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
}

/******************************************************************************
* Return the index to a specified event.
*/
QModelIndex ItemListModel::eventIndex(Entity::Id itemId) const
{
    QModelIndexList list = match(QModelIndex(), AkonadiModel::ItemIdRole, itemId, 1, Qt::MatchExactly | Qt::MatchRecursive);
    if (list.isEmpty())
        return QModelIndex();
    return index(list[0].row(), 0, list[0].parent());
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
AlarmListModel* AlarmListModel::mAllInstance = 0;

AlarmListModel::AlarmListModel(QObject* parent)
    : ItemListModel(CalEvent::ACTIVE | CalEvent::ARCHIVED, parent),
      mFilterTypes(CalEvent::ACTIVE | CalEvent::ARCHIVED)
{
}

AlarmListModel::~AlarmListModel()
{
    if (this == mAllInstance)
        mAllInstance = 0;
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
    int type = sourceModel()->data(sourceModel()->index(sourceRow, 0, sourceParent), AkonadiModel::StatusRole).toInt();
    return static_cast<CalEvent::Type>(type) & mFilterTypes;
}

bool AlarmListModel::filterAcceptsColumn(int sourceCol, const QModelIndex&) const
{
    return (sourceCol != AkonadiModel::TemplateNameColumn);
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
TemplateListModel* TemplateListModel::mAllInstance = 0;

TemplateListModel::TemplateListModel(QObject* parent)
    : ItemListModel(CalEvent::TEMPLATE, parent),
      mActionsEnabled(KAEvent::ACT_ALL),
      mActionsFilter(KAEvent::ACT_ALL)
{
}

TemplateListModel::~TemplateListModel()
{
    if (this == mAllInstance)
        mAllInstance = 0;
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
        filterChanged();
    }
}

void TemplateListModel::setAlarmActionsEnabled(KAEvent::Actions types)
{
    // Ensure that the setting isn't applied to the 'all' instance.
    if (this != mAllInstance  &&  types != mActionsEnabled)
    {
        mActionsEnabled = types;
        filterChanged();
    }
}

bool TemplateListModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (!ItemListModel::filterAcceptsRow(sourceRow, sourceParent))
        return false;
    if (mActionsFilter == KAEvent::ACT_ALL)
        return true;
    QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    KAEvent::Actions actions = static_cast<KAEvent::Actions>(sourceModel()->data(sourceIndex, AkonadiModel::AlarmActionsRole).toInt());
    return actions & mActionsFilter;
}

bool TemplateListModel::filterAcceptsColumn(int sourceCol, const QModelIndex&) const
{
    return sourceCol == AkonadiModel::TemplateNameColumn
       ||  sourceCol == AkonadiModel::TypeColumn;
}

QVariant TemplateListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal)
    {
        switch (section)
        {
            case TypeColumn:
                section = AkonadiModel::TypeColumn;
                break;
            case TemplateNameColumn:
                section = AkonadiModel::TemplateNameColumn;
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
    KAEvent::Actions actions = static_cast<KAEvent::Actions>(ItemListModel::data(index, AkonadiModel::AlarmActionsRole).toInt());
    if (!(actions & mActionsEnabled))
        f = static_cast<Qt::ItemFlags>(f & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
    return f;
}

// vim: et sw=4:
