/*
 *  collectionmodel.cpp  -  Akonadi collection models
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

#include "collectionmodel.h"

#include "akonadimodel.h"
#include "resources/resources.h"
#include "kalarm_debug.h"

#include <AkonadiCore/entitymimetypefiltermodel.h>

#include <KSharedConfig>
#include <KConfigGroup>

#include <QApplication>

using namespace Akonadi;
using namespace KAlarmCal;


CollectionControlModel*    CollectionControlModel::mInstance = nullptr;
EntityMimeTypeFilterModel* CollectionControlModel::mFilterModel;

CollectionControlModel* CollectionControlModel::instance()
{
    if (!mInstance)
        mInstance = new CollectionControlModel(qApp);
    return mInstance;
}

CollectionControlModel::CollectionControlModel(QObject* parent)
    : FavoriteCollectionsModel(AkonadiModel::instance(), KConfigGroup(KSharedConfig::openConfig(), "Collections"), parent)
{
    // Initialise the list of enabled collections
    mFilterModel = new EntityMimeTypeFilterModel(this);
    mFilterModel->addMimeTypeInclusionFilter(Collection::mimeType());
    mFilterModel->setSourceModel(AkonadiModel::instance());

    QList<Collection::Id> collectionIds;
    findEnabledCollections(QModelIndex(), collectionIds);
    setCollections(Collection::List());
    for (Collection::Id id : qAsConst(collectionIds))
        addCollection(Collection(id));

    connect(this, &QAbstractItemModel::rowsInserted,
            this, &CollectionControlModel::slotRowsInserted);

    connect(AkonadiModel::instance(), &AkonadiModel::serverStopped,
                                this, &CollectionControlModel::reset);
}

/******************************************************************************
* Recursive function to check all collections' enabled status, and to compile a
* list of all collections which have any alarm types enabled.
* Collections which duplicate the same backend storage are filtered out, to
* avoid crashes due to duplicate events in different resources.
*/
void CollectionControlModel::findEnabledCollections(const QModelIndex& parent, QList<Collection::Id>& collectionIds) const
{
    AkonadiModel* model = AkonadiModel::instance();
    for (int row = 0, count = mFilterModel->rowCount(parent);  row < count;  ++row)
    {
        const QModelIndex ix   = mFilterModel->index(row, 0, parent);
        const QModelIndex AMix = mFilterModel->mapToSource(ix);
        Resource resource = model->resource(AMix);
        if (!resource.isValid())
            continue;
        collectionIds += resource.id();
        if (mFilterModel->rowCount(ix) > 0)
            findEnabledCollections(ix, collectionIds);
    }
}

/******************************************************************************
* Called when rows have been inserted into the model.
* Connect to signals from the inserted resources.
*/
void CollectionControlModel::slotRowsInserted(const QModelIndex& parent, int start, int end)
{
    AkonadiModel* model = AkonadiModel::instance();
    for (int row = start;  row <= end;  ++row)
    {
        const QModelIndex ix   = mFilterModel->index(row, 0, parent);
        const QModelIndex AMix = mFilterModel->mapToSource(ix);
        Resource resource = model->resource(AMix);

        // Update the list of collections
        if (!collectionIds().contains(resource.id()))
            addCollection(Collection(resource.id()));   // it's a new collection.
    }
}

/******************************************************************************
* Called when the Akonadi server has stopped. Reset the model.
*/
void CollectionControlModel::reset()
{
    // Clear the collections list. This is required because addCollection() or
    // setCollections() don't work if the collections which they specify are
    // already in the list.
    setCollections(Collection::List());
}

/******************************************************************************
* Return the data for a given role, for a specified item.
*/
QVariant CollectionControlModel::data(const QModelIndex& index, int role) const
{
    return sourceModel()->data(mapToSource(index), role);
}

// vim: et sw=4:
