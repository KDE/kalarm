/*
 *  collectionmodel.h  -  Akonadi collection models
 *  Program:  kalarm
 *  Copyright © 2010-2019 David Jarvie <djarvie@kde.org>
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

#ifndef COLLECTIONMODEL_H
#define COLLECTIONMODEL_H

#include <AkonadiCore/FavoriteCollectionsModel>

namespace Akonadi
{
    class EntityMimeTypeFilterModel;
}


/*=============================================================================
= Class: CollectionControlModel
= Proxy model to select all Collections to be populated.
=============================================================================*/
class CollectionControlModel : public Akonadi::FavoriteCollectionsModel
{
        Q_OBJECT
    public:
        static CollectionControlModel* instance();

        QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const override;

    private Q_SLOTS:
        void reset();
        void slotRowsInserted(const QModelIndex& parent, int start, int end);

    private:
        explicit CollectionControlModel(QObject* parent = nullptr);
        void findEnabledCollections(const QModelIndex& parent, QList<Akonadi::Collection::Id>&) const;

        static CollectionControlModel* mInstance;
        static Akonadi::EntityMimeTypeFilterModel* mFilterModel;
};

#endif // COLLECTIONMODEL_H

// vim: et sw=4:
