/*
 *  collectionsearch.cpp  -  Search Akonadi Collections
 *  Program:  kalarm
 *  Copyright © 2014 by David Jarvie <djarvie@kde.org>
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

// kdepimlibs and kdepim-runtime 4.12.4 minimum are required
#include <kdeversion.h>
#if defined(USE_AKONADI) && KDE_IS_VERSION(4,12,4)

#include "collectionsearch.h"

#include <AkonadiCore/agentinstance.h>
#include <AkonadiCore/agentmanager.h>
#include <AkonadiCore/collectionfetchjob.h>
#include <AkonadiCore/collectionfetchscope.h>
#include <AkonadiCore/itemfetchjob.h>
#include <AkonadiCore/itemdeletejob.h>

#include <QStringList>
#include <QTimer>

using namespace Akonadi;

/******************************************************************************
* Constructor.
* Creates jobs to fetch all collections for resources containing the mime type.
* Its subsequent actions depend on the parameters:
* - If 'remove' is true, it will locate all Items with the specified 'gid' and
*   delete them. The deleted() signal will be emitted.
* - Otherwise, if 'gid' is specified, it will emit the signal items() to
*   notify all Items with that GID.
* - Otherwise, it will emit the signal collections() to notify all Collections.
*/
CollectionSearch::CollectionSearch(const QString& mimeType, const QString& gid, bool remove)
    : mMimeType(mimeType),
      mGid(gid),
      mDeleteCount(0),
      mDelete(remove && !mGid.isEmpty())
{
    const AgentInstance::List agents = AgentManager::self()->instances();
    foreach (const AgentInstance& agent, agents)
    {
        if (agent.type().mimeTypes().contains(mimeType))
        {
            {
                CollectionFetchJob* job = new CollectionFetchJob(Collection::root(), CollectionFetchJob::FirstLevel);
                job->fetchScope().setResource(agent.identifier());
                mCollectionJobs << job;
                connect(job, SIGNAL(result(KJob*)), SLOT(collectionFetchResult(KJob*)));
            }
        }
    }

    if (mCollectionJobs.isEmpty())
    {
        // There are no resources containing the mime type, so ensure that a
        // signal is emitted after construction.
        QTimer::singleShot(0, this, SLOT(finish()));
    }
}

/******************************************************************************
* Called when a CollectionFetchJob has completed.
*/
void CollectionSearch::collectionFetchResult(KJob* j)
{
    CollectionFetchJob* job = static_cast<CollectionFetchJob*>(j);
    if (j->error())
        kError() << "CollectionFetchJob" << job->fetchScope().resource() << "error: " << j->errorString();
    else
    {
        const Collection::List collections = job->collections();
        foreach (const Collection& c, collections)
        {
            if (c.contentMimeTypes().contains(mMimeType))
            {
                if (mGid.isEmpty())
                    mCollections << c;
                else
                {
                    // Search for all Items with the specified GID
                    Item item;
                    item.setGid(mGid);
                    ItemFetchJob* ijob = new ItemFetchJob(item, this);
                    ijob->setCollection(c);
                    mItemFetchJobs[ijob] = c.id();
                    connect(ijob, SIGNAL(result(KJob*)), SLOT(itemFetchResult(KJob*)));
                }
            }
        }
    }
    mCollectionJobs.removeAll(job);

    if (mCollectionJobs.isEmpty())
    {
        // All collections have now been fetched
        if (mGid.isEmpty())
            finish();
    }
}

/******************************************************************************
* Called when an ItemFetchJob has completed.
*/
void CollectionSearch::itemFetchResult(KJob* j)
{
    ItemFetchJob* job = static_cast<ItemFetchJob*>(j);
    if (j->error())
        kDebug() << "ItemFetchJob: collection" << mItemFetchJobs[job] << "GID" << mGid << "error: " << j->errorString();
    else
    {
        if (mDelete)
        {
            Item::List items = job->items();
            foreach (const Item& item, items)
            {
                ItemDeleteJob* djob = new ItemDeleteJob(item, this);
                mItemDeleteJobs[djob] = mItemFetchJobs[job];
                connect(djob, SIGNAL(result(KJob*)), SLOT(itemDeleteResult(KJob*)));
            }
        }
        else
            mItems << job->items();
    }
    mItemFetchJobs.remove(job);

    if (mItemFetchJobs.isEmpty()  &&  mItemDeleteJobs.isEmpty()  &&  mCollectionJobs.isEmpty())
        finish();  // all Items have now been fetched or deleted, so notify the result
}

/******************************************************************************
* Called when an ItemDeleteJob has completed.
*/
void CollectionSearch::itemDeleteResult(KJob* j)
{
    ItemDeleteJob* job = static_cast<ItemDeleteJob*>(j);
    if (j->error())
        kDebug() << "ItemDeleteJob: resource" << mItemDeleteJobs[job] << "GID" << mGid << "error: " << j->errorString();
    else
        ++mDeleteCount;
    mItemDeleteJobs.remove(job);

    if (mItemFetchJobs.isEmpty()  &&  mItemDeleteJobs.isEmpty()  &&  mCollectionJobs.isEmpty())
        finish();  // all Items have now been deleted, so notify the result
}

/******************************************************************************
* Notify the result of the search/delete operation, and delete this instance.
*/
void CollectionSearch::finish()
{
    if (mDelete)
        emit deleted(mDeleteCount);
    else if (mGid.isEmpty())
        emit collections(mCollections);
    else
        emit items(mItems);
    deleteLater();
}

#include "moc_collectionsearch.cpp"
#endif

// vim: et sw=4:
