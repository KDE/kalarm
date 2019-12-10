/*
 *  akonadicollectionsearch.cpp  -  Search Akonadi Collections
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

#include "akonadicollectionsearch.h"

#include "kalarm_debug.h"

#include <AkonadiCore/AgentInstance>
#include <AkonadiCore/AgentManager>
#include <AkonadiCore/CollectionFetchJob>
#include <AkonadiCore/CollectionFetchScope>
#include <AkonadiCore/ItemFetchJob>
#include <AkonadiCore/ItemFetchScope>
#include <AkonadiCore/ItemDeleteJob>
#include <KCalendarCore/Event>
using namespace KCalendarCore;

#include <QTimer>

using namespace Akonadi;

/******************************************************************************
* Constructor.
* Creates jobs to fetch all collections for resources containing the mime type.
* Its subsequent actions depend on the parameters:
* - If 'remove' is true, it will locate all Items with the specified 'gid' and
*   delete them. The deleted() signal will be emitted.
* - Otherwise, if 'gid' is specified, it will Q_EMIT the signal items() to
*   notify all Items with that GID.
* - Otherwise, it will Q_EMIT the signal collections() to notify all Collections.
*/
AkonadiCollectionSearch::AkonadiCollectionSearch(const QString& mimeType, const QString& gid, const QString& uid, bool remove)
    : mMimeType(mimeType)
    , mGid(gid)
    , mUid(uid)
    , mDelete(remove && (!mGid.isEmpty() || !mUid.isEmpty()))
{
    const AgentInstance::List agents = AgentManager::self()->instances();
    for (const AgentInstance& agent : agents)
    {
        if (agent.type().mimeTypes().contains(mimeType))
        {
            CollectionFetchJob* job = new CollectionFetchJob(Collection::root(), CollectionFetchJob::Recursive);
            job->fetchScope().setResource(agent.identifier());
            mCollectionJobs << job;
            connect(job, &CollectionFetchJob::result, this, &AkonadiCollectionSearch::collectionFetchResult);
        }
    }

    if (mCollectionJobs.isEmpty())
    {
        // There are no resources containing the mime type, so ensure that a
        // signal is emitted after construction.
        QTimer::singleShot(0, this, &AkonadiCollectionSearch::finish);
    }
}

/******************************************************************************
* Called when a CollectionFetchJob has completed.
*/
void AkonadiCollectionSearch::collectionFetchResult(KJob* j)
{
    CollectionFetchJob* job = qobject_cast<CollectionFetchJob*>(j);
    if (j->error())
        qCCritical(KALARM_LOG) << "AkonadiCollectionSearch::collectionFetchResult: CollectionFetchJob" << job->fetchScope().resource()<< "error: " << j->errorString();
    else
    {
        const Collection::List collections = job->collections();
        for (const Collection& c : collections)
        {
            if (c.contentMimeTypes().contains(mMimeType))
            {
                ItemFetchJob* ijob;
                if (!mGid.isEmpty())
                {
                    // Search for all Items with the specified GID
                    Item item;
                    item.setGid(mGid);
                    ijob = new ItemFetchJob(item, this);
                    ijob->setCollection(c);
                }
                else if (!mUid.isEmpty())
                {
                    // Search for all Events with the specified UID
                    ijob = new ItemFetchJob(c, this);
                    ijob->fetchScope().fetchFullPayload(true);
                }
                else
                {
                    mCollections << c;
                    continue;
                }
                mItemFetchJobs[ijob] = c.id();
                connect(ijob, &ItemFetchJob::result, this, &AkonadiCollectionSearch::itemFetchResult);
            }
        }
    }
    mCollectionJobs.removeAll(job);

    if (mCollectionJobs.isEmpty())
    {
        // All collections have now been fetched
        if (mGid.isEmpty() && mUid.isEmpty())
            finish();
    }
}

/******************************************************************************
* Called when an ItemFetchJob has completed.
*/
void AkonadiCollectionSearch::itemFetchResult(KJob* j)
{
    ItemFetchJob* job = qobject_cast<ItemFetchJob*>(j);
    if (j->error())
    {
        if (!mUid.isEmpty())
            qCDebug(KALARM_LOG) << "AkonadiCollectionSearch::itemFetchResult: ItemFetchJob: collection" << mItemFetchJobs[job] << "UID" << mUid << "error: " << j->errorString();
        else
            qCDebug(KALARM_LOG) << "AkonadiCollectionSearch::itemFetchResult: ItemFetchJob: collection" << mItemFetchJobs[job] << "GID" << mGid << "error: " << j->errorString();
    }
    else
    {
        if (mDelete)
        {
            const Item::List items = job->items();
            for (const Item& item : items)
            {
                if (!mUid.isEmpty())
                {
                    if (item.mimeType() == mMimeType  &&  item.hasPayload<Event::Ptr>())
                    {
                        const Event::Ptr kcalEvent = item.payload<Event::Ptr>();
                        if (kcalEvent->uid() != mUid)
                            continue;
                    }
                }
                else if (mGid.isEmpty())
                    continue;
                ItemDeleteJob* djob = new ItemDeleteJob(item, this);
                mItemDeleteJobs[djob] = mItemFetchJobs[job];
                connect(djob, &ItemDeleteJob::result, this, &AkonadiCollectionSearch::itemDeleteResult);
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
void AkonadiCollectionSearch::itemDeleteResult(KJob* j)
{
    ItemDeleteJob* job = static_cast<ItemDeleteJob*>(j);
    if (j->error())
    {
        if (!mUid.isEmpty())
            qCDebug(KALARM_LOG) << "AkonadiCollectionSearch::itemDeleteResult: ItemDeleteJob: resource" << mItemDeleteJobs[job] << "UID" << mUid << "error: " << j->errorString();
        else
            qCDebug(KALARM_LOG) << "AkonadiCollectionSearch::itemDeleteResult: ItemDeleteJob: resource" << mItemDeleteJobs[job] << "GID" << mGid << "error: " << j->errorString();
    }
    else
        ++mDeleteCount;
    mItemDeleteJobs.remove(job);

    if (mItemFetchJobs.isEmpty()  &&  mItemDeleteJobs.isEmpty()  &&  mCollectionJobs.isEmpty())
        finish();  // all Items have now been deleted, so notify the result
}

/******************************************************************************
* Notify the result of the search/delete operation, and delete this instance.
*/
void AkonadiCollectionSearch::finish()
{
    if (mDelete)
        Q_EMIT deleted(mDeleteCount);
    else if (mGid.isEmpty() && mUid.isEmpty())
        Q_EMIT collections(mCollections);
    else
        Q_EMIT items(mItems);
    deleteLater();
}

// vim: et sw=4:
