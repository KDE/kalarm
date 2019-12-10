/*
 *  akonadicollectionsearch.h  -  Search Akonadi Collections
 *  Program:  kalarm
 *  Copyright © 2014,2019 David Jarvie <djarvie@kde.org>
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

#ifndef AKONADICOLLECTIONSEARCH_H
#define AKONADICOLLECTIONSEARCH_H


#include <AkonadiCore/Collection>
#include <AkonadiCore/Item>

#include <QObject>
#include <QMap>
#include <QList>

class KJob;
namespace Akonadi
{
class CollectionFetchJob;
class ItemFetchJob;
class ItemDeleteJob;
}

/*=============================================================================
= Class: AkonadiCollectionSearch
= Fetches a list of all Akonadi collections which handle a specified mime type,
= and then optionally fetches or deletes all Items from them with a given GID
= or UID.
=
= Note that this class auto-deletes once it has emitted its completion signal.
= Instances must therefore be created on the heap by operator new(), not on the
= stack.
=============================================================================*/
class AkonadiCollectionSearch : public QObject
{
        Q_OBJECT
    public:
        explicit AkonadiCollectionSearch(const QString& mimeType, const QString& gid = QString(), const QString& uid = QString(), bool remove = false);

    Q_SIGNALS:
        // Signal emitted if action is to fetch all collections for the mime type
        void collections(const Akonadi::Collection::List&);
        // Signal emitted if action is to fetch all items with the remote ID
        void items(const Akonadi::Item::List&);
        // Signal emitted if action is to delete all items with the remote ID
        void deleted(int count);

    private Q_SLOTS:
        void collectionFetchResult(KJob*);
        void itemFetchResult(KJob*);
        void itemDeleteResult(KJob*);
        void finish();

    private:
        QString                                mMimeType;
        QString                                mGid;
        QString                                mUid;
        QList<Akonadi::CollectionFetchJob*>    mCollectionJobs;
        QMap<Akonadi::ItemFetchJob*, Akonadi::Collection::Id>  mItemFetchJobs;
        QMap<Akonadi::ItemDeleteJob*, Akonadi::Collection::Id> mItemDeleteJobs;
        Akonadi::Collection::List              mCollections;
        Akonadi::Item::List                    mItems;
        int                                    mDeleteCount{0};
        bool                                   mDelete;
};

#endif // AKONADICOLLECTIONSEARCH_H

// vim: et sw=4:
