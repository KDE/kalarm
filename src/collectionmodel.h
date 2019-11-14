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

#include "resources/resource.h"

#include <kalarmcal/kacalendar.h>

#include <AkonadiCore/FavoriteCollectionsModel>

using namespace KAlarmCal;

namespace Akonadi
{
    class EntityMimeTypeFilterModel;
}


/*=============================================================================
= Class: CollectionControlModel
= Proxy model to select which Collections will be enabled. Disabled Collections
= are not populated or monitored; their contents are ignored. The set of
= enabled Collections is stored in the config file's "Collections" group.
= Note that this model is not used directly for displaying - its purpose is to
= allow collections to be disabled, which will remove them from the other
= collection models.
=============================================================================*/
class CollectionControlModel : public Akonadi::FavoriteCollectionsModel
{
        Q_OBJECT
    public:
        static CollectionControlModel* instance();

        /** Find the collection to be used to store an event of a given type.
         *  This will be the standard collection for the type, but if this is not valid,
         *  the user will be prompted to select a collection.
         *  @param type         The event type
         *  @param promptParent The parent widget for the prompt
         *  @param noPrompt     Don't prompt the user even if the standard collection is not valid
         *  @param cancelled    If non-null: set to true if the user cancelled the
         *                      prompt dialogue; set to false if any other error
         */
        static Resource destination(CalEvent::Type type, QWidget* promptParent = nullptr, bool noPrompt = false, bool* cancelled = nullptr);

        /** Return a list of all collections, both enabled and disabled.
         *  Collections which don't belong to a resource are omitted.
         *  @param type  Return only collections for this event type, or EMPTY for all.
         */
        static Akonadi::Collection::List allCollections(CalEvent::Type type = CalEvent::EMPTY);

        /** Return the collection ID for a given Akonadi resource ID.
         *  @return  collection ID, or -1 if the resource is not in KAlarm's list.
         */
        static Akonadi::Collection::Id collectionForResourceName(const QString& resourceName);

        /** Check for, and remove, Akonadi resources which duplicate use of
         *  calendar files/directories.
         */
        static void removeDuplicateResources();

        QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const override;

    private Q_SLOTS:
        void reset();
        void slotRowsInserted(const QModelIndex& parent, int start, int end);
        void resourceStatusChanged(Resource&, ResourceType::Changes);
        void collectionFetchResult(KJob*);

    private:
        explicit CollectionControlModel(QObject* parent = nullptr);
        void findEnabledCollections(const QModelIndex& parent, QList<Akonadi::Collection::Id>&) const;
        CalEvent::Types setEnabledStatus(Resource&, CalEvent::Types, bool inserted);
        static CalEvent::Types checkTypesToEnable(const Resource&, const QList<Akonadi::Collection::Id>&, CalEvent::Types);

        static CollectionControlModel* mInstance;
        struct ResourceCol
        {
            QString                 resourceId;
            Akonadi::Collection::Id collectionId;
            ResourceCol() {}
            ResourceCol(const QString& r, Akonadi::Collection::Id c)
                : resourceId(r), collectionId(c) {}
        };
        static QHash<QString, ResourceCol> mAgentPaths;   // path, (resource identifier, collection ID) pairs
        static Akonadi::EntityMimeTypeFilterModel* mFilterModel;
};

#endif // COLLECTIONMODEL_H

// vim: et sw=4:
